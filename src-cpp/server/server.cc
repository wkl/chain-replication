#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<ChainServer> cs;

// global
int send_msg_seq = 0;
int rec_msg_seq = 0;

void ChainServerUDPLoop::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl << endl;
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
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl << endl;
      break;
  }
}

void ChainServer::forward_request(const proto::Request &req) {
  cout << "forwarding request..." << endl;
  send_msg_tcp(succ_server_addr_, proto::Message::REQUEST, req);
}

void ChainServer::reply(const proto::Request& req) {
  proto::Reply reply = req.reply();
  proto::Address client;
  client.set_ip(req.client_addr().ip());
  client.set_port(req.client_addr().port());
  send_msg_udp(local_addr_, client, proto::Message::REPLY, reply);
}

void ChainServer::sendback_ack(const proto::Acknowledge& ack) {
  cout << "sending back acknowledge..." << endl;
  send_msg_tcp(pre_server_addr_, proto::Message::ACKNOWLEDGE, ack);
}

void ChainServer::receive_ack(proto::Acknowledge* ack) {
  while (!sent_req_list_.empty() && sent_req_list_.front().bank_update_seq()<=ack->bank_update_seq()) {
    proto::Request req = sent_req_list_.front();
    auto it = (processed_update_map_).find(req.req_id());
    if (it == (processed_update_map_).end()) {  // request doesn't exists in processed_update_map_
      auto it_insert = processed_update_map_.insert(std::make_pair(req.req_id(), req));
      assert(it_insert.second);
    }
    sent_req_list_.pop_front();
  }
  cout << "length of sent_req_list: " << sent_req_list_.size() << ", length of processed_update_map: " << processed_update_map_.size() << endl;
  if (!ishead_) {
    sendback_ack(*ack);
  }
}

void ChainServer::receive_request(proto::Request* req) {
  // processing query request
  if (req->type() == proto::Request::QUERY) {
    assert(istail_);
    handle_query(req);
    return;
  }
  // processing update request
  assert(req->type() != proto::Request::QUERY);
  // head server
  if (ishead_ && !istail_) {
    bank_update_seq_ ++;        // sequence of update request
    req->set_bank_update_seq(bank_update_seq_);
    head_handle_update(req);
    return;
  }
  // only one server
  if (ishead_ && istail_) {        
    bank_update_seq_ ++;        // sequence of update request
    req->set_bank_update_seq(bank_update_seq_);
    single_handle_update(req);
    return;
  }
  // ignore duplicate update request which is possible when handling failure
  if (bank_update_seq_ >= req->bank_update_seq()) {
        return;
  }
  // assertion based on our FIFO request forwarding assumption
  assert(bank_update_seq_ == req->bank_update_seq() - 1);
  bank_update_seq_ = req->bank_update_seq();
  // tail server
  if (!ishead_ && istail_) {
    tail_handle_update(req);
    return;
  }
  // internal server
  assert(!ishead_ && !istail_);
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
  cout << "Processed request result: req_id=" << req->reply().req_id() << ", balance=" << req->reply().balance() << endl;
  insert_sent_req_list(*req);
  forward_request(*req);
}

// single server handle update request
void ChainServer::single_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout << "Processed request result: req_id=" << req->reply().req_id() << ", balance=" << req->reply().balance() << endl;
  if (req->check_result() == proto::Request::NEWREQ) {
    update_processed_update_list(*req);
  }
  cs->reply(*req);
}

// tail server handle update request
void ChainServer::tail_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout << "Processed request result: req_id=" << req->reply().req_id() << ", balance=" << req->reply().balance() << endl;
  if (req->check_result() == proto::Request::NEWREQ) {
    update_processed_update_list(*req);
  }
  if (req->type() != proto::Request::TRANSFERTO) {
    cout << "client ip: " << req->client_addr().ip() << ", port: " << req->client_addr().port() << endl;
    reply(*req);
  }
  proto::Acknowledge ack;
  ack.set_bank_update_seq(req->bank_update_seq());
  sendback_ack(ack);
}

// interval server handle update request
void ChainServer::internal_handle_update(proto::Request* req) {
  get_update_req_result(req);
  cout << "Processed request result: req_id=" << req->reply().req_id() << ", balance=" << req->reply().balance() << endl;
  insert_sent_req_list(*req);
  forward_request(*req);
}

// used in all the xxxx_handle_update
void ChainServer::get_update_req_result(proto::Request* req) {
  proto::Reply* reply = new proto::Reply;
  proto::Request_CheckRequest check_result = check_update_request(*req);
  req->set_check_result(check_result);
  switch (check_result) {
    case proto::Request::INCONSISTENT:
      reply->set_outcome(proto::Reply::INCONSISTENT_WITH_HISTORY); 
      break;
    case proto::Request::PROCESSED:
      reply->set_outcome(proto::Reply::PROCESSED);
      break;
    case proto::Request::NEWREQ:
      ChainServer::UpdateBalanceOutcome update_result = update_balance(*req);
      if (update_result == ChainServer::UpdateBalanceOutcome::InsufficientFunds) {    
        reply->set_outcome(proto::Reply::INSUFFICIENT_FUNDS);
      } else {
        reply->set_outcome(proto::Reply::PROCESSED);
      }
      break;
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
  } else {
    auto it = (processed_update_map_).find(req.req_id());
    if (it == (processed_update_map_).end()) {  // request doesn't exist in processed_update_map_
      if ((sent_req_list_).size() > 0) {
        for(auto it_queue=(sent_req_list_).begin(); it_queue!=(sent_req_list_).end(); ++it_queue) {
          if (req.req_id() == (*it_queue).req_id()) { // request exists in sent_req_list_
	    bool if_req_consistent = check_req_consistency(req, *it_queue);
            if (if_req_consistent) {	// consistent request
              return proto::Request::PROCESSED;
            } else { 	// inconsistent request
              return proto::Request::INCONSISTENT;
            }
          }
        }
      }
      // request doesn't exist in sent_req_list_
      return proto::Request::NEWREQ;
    } else {  // request exists in processed_update_map_
      bool if_req_consistent = check_req_consistency(req, (*it).second);
      if (if_req_consistent) {	// consistent request
        return proto::Request::PROCESSED;
      } else { 	// inconsistent request
        return proto::Request::INCONSISTENT;
      }
    }
  }
}

// get or create account object
Account& ChainServer::get_or_create_account(const proto::Request& req, bool* ifexisted_account) {
  string account_id = req.account_id();
  auto it = bank_.account_map().find(account_id);
  if (it == bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it_insert = bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it_insert.second);
    *ifexisted_account = false;
    return (it_insert.first)->second;
  } else {  // get the account
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
  } else {
    return false;
  }
}

// used in handle_query
float ChainServer::get_balance(string account_id) {
  float balance = 0;
  auto it = bank_.account_map().find(account_id);
  if (it == bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it2 = bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it2.second);
  } else {  // get balance of the account
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
    } else {
      account.set_balance(account.balance() - req.amount());  
      return ChainServer::UpdateBalanceOutcome::Success;
    }
  } else {
    account.set_balance(account.balance() + req.amount());  
    return ChainServer::UpdateBalanceOutcome::Success;
  }
}

// used in tail_handle_update(req) and single_handle_update(req)
void ChainServer::update_processed_update_list(const proto::Request& req) {
  auto it = processed_update_map_.insert(std::make_pair(req.req_id(), req));
  assert(it.second);
}

// used in head_handle_update(req) and interval_handle_update(req)
void ChainServer::insert_sent_req_list(const proto::Request& req) {
  sent_req_list_.push_back(req);	// insert at the end of deque
}

// read configuration file for a server
int read_config_server(string dir, string bankid, int chainno) {
  Json::Reader reader;
  Json::Value root;
  std::ifstream ifs;
  ifs.open(dir, std::ios::binary);
  if (!ifs.is_open()) { 
    LOG(ERROR) << "Fail to open the config file: " << dir << endl << endl; 
    exit (1); 
  } 
  if (reader.parse(ifs,root)) { 
    Json::Value bank_list_json = root[JSON_BANKS];
    for (int i = 0; i < bank_list_json.size(); i++) {
      if (bank_list_json[i][JSON_BANKID].asString() == bankid) {
        Json::Value bank_json = bank_list_json[i];
        Json::Value server_list_json = bank_json[JSON_SERVERS];
	Json::Value server_json;
	bool find_res = get_server_json_with_chainno(server_list_json, server_json, chainno);
	if (find_res) {
	  cs->set_bank_id(bankid);
	  Bank bank(bankid);
	  cs->set_bank(bank);    
	  cs->set_ishead(false);
	  cs->set_istail(false);
	  if (chainno == 1) {	// check chainno started from 1?
	    cs->set_ishead(true);
	  }
	  if (chainno == server_list_json.size()) {
	    cs->set_istail(true);
	  }
	  proto::Address local_addr;  // local address
          local_addr.set_ip(server_json[JSON_IP].asString());
          local_addr.set_port(server_json[JSON_PORT].asInt());
	  cs->set_local_addr(local_addr);
	  if (!cs->ishead()) { // pre_server address, check continuous chainno?
	    Json::Value pre_server_json;
	    bool find_res = get_server_json_with_chainno(server_list_json, pre_server_json, chainno-1);
	    assert(find_res);
            proto::Address pre_server;  
      	    pre_server.set_ip(pre_server_json[JSON_IP].asString());
            pre_server.set_port(pre_server_json[JSON_PORT].asInt());
            cs->set_pre_server_addr(pre_server);
	  }
	  if (!cs->istail()) { // succ_server address, check continuous chainno?
	    Json::Value succ_server_json;
	    bool find_res = get_server_json_with_chainno(server_list_json, succ_server_json, chainno+1);
	    assert(find_res);
            proto::Address succ_server;  
      	    succ_server.set_ip(succ_server_json[JSON_IP].asString());
            succ_server.set_port(succ_server_json[JSON_PORT].asInt());
            cs->set_succ_server_addr(succ_server);
	  }
	  goto READ_FINISH;        
	} else {
          ifs.close();
          LOG(ERROR) << "The chainno is not in the config file: " << dir << endl << endl; 
          exit (1);
	}
      }
    }
    ifs.close();
    LOG(ERROR) << "The bankid specified is not involved in the config file: " << dir << endl << endl; 
    exit (1);
  } else {
    ifs.close();
    LOG(ERROR) << "Fail to parse the config file: " << dir << endl << endl; 
    exit (1); 
  }

READ_FINISH:
  LOG_IF(INFO, (cs->ishead() && !cs->istail())) << "New server initialization: bankid=" << cs->bank_id() << ", head server"
	 << ", local ip=" << cs->local_addr().ip() << ", local port=" << cs->local_addr().port()
	 << ", succ_server ip=" << cs->succ_server_addr().ip() << ", succ_server port=" << cs->succ_server_addr().port() << endl << endl;
  LOG_IF(INFO, (!cs->ishead() && cs->istail())) << "New server initialization: bankid=" << cs->bank_id() << ", tail server"
	 << ", local ip=" << cs->local_addr().ip() << ", local port=" << cs->local_addr().port()
	 << ", pre_server ip=" << cs->pre_server_addr().ip() << ", pre_server port=" << cs->pre_server_addr().port() << endl << endl;
  LOG_IF(INFO, (!cs->ishead() && !cs->istail())) << "New server initialization: bankid=" << cs->bank_id() << ", internal server"
	 << ", local ip=" << cs->local_addr().ip() << ", local port=" << cs->local_addr().port()
	 << ", pre_server ip=" << cs->pre_server_addr().ip() << ", pre_server port=" << cs->pre_server_addr().port()  
	 << ", succ_server ip=" << cs->succ_server_addr().ip() << ", succ_server port=" << cs->succ_server_addr().port() << endl << endl;
  LOG_IF(INFO, (cs->ishead() && cs->istail())) << "New server initialization: bankid=" << cs->bank_id() << ", single server"
	 << ", local ip=" << cs->local_addr().ip() << ", local port=" << cs->local_addr().port() << endl << endl; 

  ifs.close();
  return 0;
}

bool get_server_json_with_chainno(Json::Value server_list_json, Json::Value& result_server_json, int chainno) {
  for (int j = 0; j < server_list_json.size(); j++) {
    if (server_list_json[j][JSON_CHAINNO].asInt() == chainno) {
      result_server_json = server_list_json[j];
      return true;
    }
  }
  return false;
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<string>(), "specify the config-file path")(
        "log-dir,l", po::value<string>(), "specify logging dir")(
        "bank-id,b", po::value<string>(), "specify the bank id")(
	"chain-no,n", po::value<int>(), "specify the server sequence in the chain");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << endl;
      return 0;
    }

    FLAGS_logtostderr = true;
    if (vm.count("log-dir") && vm.count("config-file") && vm.count("bank-id") && vm.count("chain-no")) {
      //FLAGS_log_dir = vm["log-dir"].as<string>();
      string log_dir = vm["log-dir"].as<std::string>() + "/server";
      mkdir(log_dir.c_str(), 0777);
      FLAGS_log_dir = log_dir;	
      FLAGS_logtostderr = false;
      FLAGS_logbuflevel = -1;
      string log_name = "server_" + vm["bank-id"].as<string>() + "_No" + std::to_string(vm["chain-no"].as<int>());
      google::InitGoogleLogging(log_name.c_str());
    } else {
      FLAGS_logbuflevel = -1;
      google::InitGoogleLogging(argv[0]);
    }

    LOG(INFO) << "Processing configuration file" << endl << endl;

    cs = std::unique_ptr<ChainServer>(new ChainServer());

    if (vm.count("config-file") && vm.count("bank-id") && vm.count("chain-no")) {
      read_config_server(vm["config-file"].as<string>(), vm["bank-id"].as<string>(), vm["chain-no"].as<int>());
      ChainServerUDPLoop udp_loop(cs->local_addr().port());
      ChainServerTCPLoop tcp_loop(cs->local_addr().port());
      std::thread udp_thread(udp_loop);
      std::thread tcp_thread(tcp_loop);
      udp_thread.join();
      tcp_thread.join();
    }
    else {
      LOG(ERROR) << "Please input the config-file path, bankid and the server sequence in the chain" << endl << endl;
      return 1;
    }

  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}

