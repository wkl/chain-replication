#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<ChainServer> cs;

void ChainServerUDPLoop::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServerTCPLoop::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    case proto::Message::ACKNOWLEDGE:
      assert(msg.has_ack());
      cs->receive_ack(msg.mutable_ack());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServer::forward_request(const proto::Request &req) {
  std::cout << "forwarding request..." << std::endl;
  send_msg_tcp(cs->succ_server_addr_, proto::Message::REQUEST, req);
}

void ChainServer::reply(const proto::Request& req) {
  proto::Reply reply = req.reply();
  proto::Address client;
  client.set_ip(req.client_addr().ip());
  client.set_port(req.client_addr().port());
  send_msg_udp(client, proto::Message::REPLY, reply);
}

void ChainServer::sendback_ack(const proto::Acknowledge& ack) {
  std::cout << "sending back acknowledge..." << std::endl;
  send_msg_tcp(cs->pre_server_addr_, proto::Message::ACKNOWLEDGE, ack);
}

void ChainServer::receive_ack(proto::Acknowledge* ack) {
  while (!cs->sent_req_list_.empty() && cs->sent_req_list_.front().bank_update_seq()<=ack->bank_update_seq()) {
    proto::Request req = cs->sent_req_list_.front();
    auto it = (cs->processed_update_map_).find(req.req_id());
    if (it == (cs->processed_update_map_).end()) {  // request doesn't exists in processed_update_map_
      auto it_insert = cs->processed_update_map_.insert(std::make_pair(req.req_id(), req));
      assert(it_insert.second);
    }
    cs->sent_req_list_.pop_front();
  }
  cout<<"length of sent_req_list: "<<cs->sent_req_list_.size()<<", length of processed_update_map: "<<cs->processed_update_map_.size()<<endl;
  if (!cs->ishead_) {
    sendback_ack(*ack);
  }
}

void ChainServer::receive_request(proto::Request* req) {
  // processing query request
  if (req->type() == proto::Request::QUERY) {
    assert(cs->istail_);
    handle_query(req);
    return;
  }
  // processing update request
  assert(req->type() != proto::Request::QUERY);
  // head server
  if (cs->ishead_ && !cs->istail_) {
    cs->bank_update_seq_ ++;        // sequence of update request
    req->set_bank_update_seq(cs->bank_update_seq_);
    head_handle_update(req);
    return;
  }
  // only one server
  if (cs->ishead_ && cs->istail_) {        
    cs->bank_update_seq_ ++;        // sequence of update request
    req->set_bank_update_seq(cs->bank_update_seq_);
    single_handle_update(req);
    return;
  }
  // ignore duplicate update request which is possible when handling failure
  if (cs->bank_update_seq_ >= req->bank_update_seq()) {
        return;
  }
  // assertion based on our FIFO request forwarding assumption
  assert(cs->bank_update_seq_ == req->bank_update_seq() - 1);
  cs->bank_update_seq_ = req->bank_update_seq();
  // tail server
  if (!cs->ishead_ && cs->istail_) {
    tail_handle_update(req);
    return;
  }
  // internal server
  assert(!cs->ishead_ && !cs->istail_);
  internal_handle_update(req);
  return;
}

// handle query request
void ChainServer::handle_query(proto::Request* req) {
  float balance = get_balance(req->account_id());
  proto::Reply* reply = new proto::Reply;
  reply->set_outcome(proto::Reply::PROCESSED);
  reply->set_req_id(req->req_id());
  reply->set_balance(balance);
  req->set_allocated_reply(reply);
  cs->reply(*req);
}

// head server handle update request
void ChainServer::head_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout<<"Processed request result: req_id="<<req->reply().req_id()<<", balance="<<req->reply().balance()<<endl;
  insert_sent_req_list(*req);
  cs->forward_request(*req);
}

// single server handle update request
void ChainServer::single_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout<<"Processed request result: req_id="<<req->reply().req_id()<<", balance="<<req->reply().balance()<<endl;
  if (req->check_result() == proto::Request::NEWREQ) {
    update_processed_update_list(*req);
  }
  cs->reply(*req);
}

// tail server handle update request
void ChainServer::tail_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout<<"Processed request result: req_id="<<req->reply().req_id()<<", balance="<<req->reply().balance()<<endl;
  if (req->check_result() == proto::Request::NEWREQ) {
    update_processed_update_list(*req);
  }
  if (req->type() != proto::Request::TRANSFERTO) {
    cout<<"client ip: "<<req->client_addr().ip()<<", port: "<<req->client_addr().port()<<endl;
    cs->reply(*req);
  }
  proto::Acknowledge ack;
  ack.set_bank_update_seq(req->bank_update_seq());
  sendback_ack(ack);
}

// interval server handle update request
void ChainServer::internal_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout<<"Processed request result: req_id="<<req->reply().req_id()<<", balance="<<req->reply().balance()<<endl;
  insert_sent_req_list(*req);
  cs->forward_request(*req);
}

// used in all the xxxx_handle_update
void ChainServer::get_update_req_result(proto::Request* req) {
  proto::Reply* reply = new proto::Reply;
  proto::Request_CheckRequest check_result = check_update_request(*req);
  req->set_check_result(check_result);
  if (check_result == proto::Request::INCONSISTENT) {
    reply->set_outcome(proto::Reply::INCONSISTENT_WITH_HISTORY); 
  }
  else if (check_result == proto::Request::PROCESSED) {
    reply->set_outcome(proto::Reply::PROCESSED);
  }
  else { //check_result == proto::Request::NEWREQ
    ChainServer::UpdateBalanceOutcome update_result = update_balance(*req);
    if (update_result == ChainServer::UpdateBalanceOutcome::InsufficientFunds) {    
      reply->set_outcome(proto::Reply::INSUFFICIENT_FUNDS);
    }
    else {
      reply->set_outcome(proto::Reply::PROCESSED);
    }
  }
  reply->set_req_id(req->req_id());
  float balance = get_balance(req->account_id());
  reply->set_balance(balance);
  req->set_allocated_reply(reply);
}

// used in get_update_req_result
proto::Request_CheckRequest ChainServer::check_update_request(const proto::Request& req) {
  bool ifexisted_account = true;
  get_or_create_account(req, &ifexisted_account); 

  if (!ifexisted_account) {
    return proto::Request::NEWREQ;
  }
  else {
    auto it = (cs->processed_update_map_).find(req.req_id());
    if (it == (cs->processed_update_map_).end()) {  // request doesn't exist in processed_update_map_
      if ((cs->sent_req_list_).size() > 0) {
        for(auto it_queue=(cs->sent_req_list_).begin(); it_queue!=(cs->sent_req_list_).end(); ++it_queue) {
          if (req.req_id() == (*it_queue).req_id()) { // request exists in sent_req_list_
	    bool if_req_consistent = check_req_consistency(req, *it_queue);
            if (if_req_consistent) {	// consistent request
              return proto::Request::PROCESSED;
            }
            else { 	// inconsistent request
              return proto::Request::INCONSISTENT;
            }
          }
        }
      }
      // request doesn't exist in sent_req_list_
      return proto::Request::NEWREQ;
    }
    else {  // request exists in processed_update_map_
      bool if_req_consistent = check_req_consistency(req, (*it).second);
      if (if_req_consistent) {	// consistent request
        return proto::Request::PROCESSED;
      }
      else { 	// inconsistent request
        return proto::Request::INCONSISTENT;
      }
    }
  }
}

// get or create account object
Account& ChainServer::get_or_create_account(const proto::Request& req, bool* ifexisted_account) {
  string account_id = req.account_id();
  auto it = cs->bank_.account_map().find(account_id);
  if (it == cs->bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it_insert = cs->bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it_insert.second);
    *ifexisted_account = false;
    return (it_insert.first)->second;
  }
  else {  // get the account
    *ifexisted_account = true;
    return (*it).second;
  }  
}

// check if the contents of 2 requests with same req_id are consistent
bool ChainServer::check_req_consistency(const proto::Request& req1, const proto::Request& req2) {
  // hasn't compared client addr
  if ((req1.type() == req2.type()) && (req1.account_id() == req2.account_id()) && (req1.bank_id() == req2.bank_id()) && (req1.amount() == req2.amount())
   && (req1.dest_bank_id() == req2.dest_bank_id()) && (req1.dest_account_id() == req2.dest_account_id())) {
    return true;
  }
  else {
    return false;
  }
}

// used in handle_query
float ChainServer::get_balance(string account_id) {
  float balance = 0;
  auto it = cs->bank_.account_map().find(account_id);
  if (it == cs->bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it2 = cs->bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it2.second);
  }
  else {  // get balance of the account
    balance = (*it).second.balance();
  }
  return balance;
}

// used in check_update_request
ChainServer::UpdateBalanceOutcome ChainServer::update_balance(const proto::Request& req) {
  bool ifexisted_account = true;
  Account& account = get_or_create_account(req, &ifexisted_account);
  if (req.type() == proto::Request::WITHDRAW || req.type() == proto::Request::TRANSFER) {
    if (account.balance() < req.amount()) {
      return ChainServer::UpdateBalanceOutcome::InsufficientFunds;
    }
    else {
      account.set_balance(account.balance() - req.amount());  
      return ChainServer::UpdateBalanceOutcome::Success;
    }
  }
  else {
    account.set_balance(account.balance() + req.amount());  
    return ChainServer::UpdateBalanceOutcome::Success;
  }
}

// used in tail_handle_update(req) and single_handle_update(req)
void ChainServer::update_processed_update_list(const proto::Request& req) {
  auto it = cs->processed_update_map_.insert(std::make_pair(req.req_id(), req));
  assert(it.second);
}

// used in head_handle_update(req) and interval_handle_update(req)
void ChainServer::insert_sent_req_list(const proto::Request& req) {
  cs->sent_req_list_.push_back(req);	// insert at the end of deque
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<std::string>(), "specify config file path")(
        "log-dir,l", po::value<std::string>(), "specify logging dir")(
        "second,s", "I'm second chain server")("third,t", "I'm third chain server");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    FLAGS_logtostderr = true;
    if (vm.count("log-dir")) {
      FLAGS_log_dir = vm["log-dir"].as<std::string>();
      FLAGS_logtostderr = false;
    }
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "Processing configuration file";

    cs = std::unique_ptr<ChainServer>(new ChainServer("bank1"));

    int server_port;
    if (vm.count("second")) {	// internal server
      cs->set_ishead(false);
      cs->set_istail(false);
      server_port = 50002;
      proto::Address pre_server;  // pre_server address
      pre_server.set_ip("127.0.0.1");
      pre_server.set_port(50001);
      cs->set_pre_server_addr(pre_server);
      proto::Address succ_server;  // succ_server address
      succ_server.set_ip("127.0.0.1");
      succ_server.set_port(50003);
      cs->set_succ_server_addr(succ_server);
    } else if (vm.count("third")) {	// tail server
      cs->set_ishead(false);
      cs->set_istail(true);
      server_port = 50003;
      proto::Address pre_server;  // pre_server address
      pre_server.set_ip("127.0.0.1");
      pre_server.set_port(50002);
      cs->set_pre_server_addr(pre_server);
    } else {	// head server
      cs->set_ishead(true);
      cs->set_istail(false);
      server_port = 50001;
      proto::Address succ_server;  // succ_server address
      succ_server.set_ip("127.0.0.1");
      succ_server.set_port(50002);
      cs->set_succ_server_addr(succ_server);
    }

    if (vm.count("config-file")) {
      std::ifstream ifs(vm["config-file"].as<std::string>(), std::ios::binary);
      if (!ifs) {
        std::cerr << "open '" << vm["config-file"].as<std::string>() << "' failed"
                  << std::endl;
        return 1;
      }

      Json::Reader reader;
      Json::Value root;
      assert(reader.parse(ifs, root));
    }
    /*
    else {
      cerr << "config-file was not set." << endl;
      return 1;
    }
    */

    ChainServerUDPLoop udp_loop(server_port);
    ChainServerTCPLoop tcp_loop(server_port);
    std::thread udp_thread(udp_loop);
    std::thread tcp_thread(tcp_loop);
    udp_thread.join();
    tcp_thread.join();

  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

