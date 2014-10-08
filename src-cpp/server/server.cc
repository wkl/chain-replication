#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<ChainServer> cs;

// variables for debug
bool is_head;
bool is_tail;

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
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServer::forward_request(proto::Request* req) {
  req->set_bank_update_seq(++bank_update_seq_);
  std::cout << "forwarding request..." << std::endl;
  proto::Address addr;
  addr.set_ip("127.0.0.1");
  addr.set_port(50002);
  send_msg_tcp(addr, proto::Message::REQUEST, *req);
}

void ChainServer::reply(const proto::Request& req) {
  proto::Reply reply;
  reply.set_outcome(proto::Reply::PROCESSED);
  reply.set_req_id(req.req_id());
  proto::Address client;
  client.set_ip(req.client_addr().ip());
  client.set_port(req.client_addr().port());
  send_msg_udp(client, proto::Message::REPLY, reply);
}

void ChainServer::receive_request(proto::Request* request) {
  //if (is_head) cs->forward_request(req);
  //if (is_tail) cs->reply(*req);
  if (request->type() == proto::Request::QUERY) {
        assert(cs->istail_);
        handle_query(*request);
  }
  
  // processing update request
  assert(request->type() != proto::Request::QUERY);

  // only one server
  if (cs->ishead_ && cs->istail_) {        
        cs->bank_update_seq_ ++;        // sequence of update request
        request->set_bank_update_seq(cs->bank_update_seq_);
        single_handle_update(request);
  }
}

// handle query request
void ChainServer::handle_query(const proto::Request& request) {
  float balance = get_balance(request.account_id());

  proto::Reply reply;
  reply.set_outcome(proto::Reply::PROCESSED);
  reply.set_req_id(request.req_id());
  reply.set_balance(balance);
  proto::Address client;
  client.set_ip(request.client_addr().ip());
  client.set_port(request.client_addr().port());
  send_msg_udp(client, proto::Message::REPLY, reply);
}

// single server handle update request
void ChainServer::single_handle_update(proto::Request* request) {
  proto::Reply req_result = get_update_req_result(request);
}

// used in all the xxxx_handle_update
proto::Reply ChainServer::get_update_req_result(proto::Request* request) {
  proto::Reply req_result;
  check_update_request(request);

    /*
    check_result = check_update_request(request)
    if (check_result == CheckRequest.Inconsistent):
            req_result = ReqResult(request.req_id,  RequestOutcome.InconsistentWithHistory, self.balance)
    elif (check_result == CheckRequest.Processed):
            req_result = ReqResult(request.req_id, RequestOutcome.Processed, self.balance)
    else:
            update_result = update_balance(request)
            if (update_result == UpdateBalanceOutcome.InsufficientFunds):
            	req_result = ReqResult(request.req_id,
                         RequestOutcome.InsufficientFunds, self.balance)
            else:
                req_result = ReqResult(request.req_id,
                         RequestOutcome.Processed, self.balance)
    return req_result
    */

  return req_result;
}

// used in get_update_req_result
ChainServer::CheckRequest ChainServer::check_update_request(proto::Request* request) {
  bool ifexisted_account = true;
  Account& account = get_or_create_account(request, &ifexisted_account);
  cout<<"balance: "<<account.balance()<<", ifexisted:"<<ifexisted_account<<endl;
  if (!ifexisted_account) {
    return ChainServer::CheckRequest::NewReq;
  }
  else {
    unordered_map<string,proto::Request>::iterator it;
    it = (cs->processed_update_map_).find(request->req_id());
    if (it == (cs->processed_update_map_).end()) {  // request doesn't exist in processed_update_map_
      if ((cs->sent_req_list_).size() > 0) {
        for(auto itq=(cs->sent_req_list_).begin(); itq!=(cs->sent_req_list_).end(); ++itq) {
          if (request->req_id() == (*itq).req_id()) { // request exists in sent_req_list_
	    bool if_req_consistent = check_req_consistency(*request, *itq);
            if (if_req_consistent) {	// consistent request
              return ChainServer::CheckRequest::Processed;
            }
            else { 	// inconsistent request
              return ChainServer::CheckRequest::Inconsistent;
            }
          }
        }
      }
      // request doesn't exist in sent_req_list_
      return ChainServer::CheckRequest::NewReq;
    }
    else {  // request exists in processed_update_map_
      bool if_req_consistent = check_req_consistency(*request, (*it).second);
      if (if_req_consistent) {	// consistent request
        return ChainServer::CheckRequest::Processed;
      }
      else { 	// inconsistent request
        return ChainServer::CheckRequest::Inconsistent;
      }
    }
  }
}

// get or create account object
Account& ChainServer::get_or_create_account(proto::Request* request, bool* ifexisted_account) {
  // in Phase 4, need to deal with transfer type: get dest account
  string account_id = request->account_id();
  unordered_map<string, Account>::iterator it;
  it = cs->bank_.account_map().find(account_id);
  if (it == cs->bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    std::pair<string,Account> pair_input (account_id, account);
    auto pair = cs->bank_.account_map().insert(pair_input); 
    assert(pair.second);
    *ifexisted_account = false;
    return (pair.first)->second;
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
  unordered_map<string,Account>::iterator it;
  it = cs->bank_.account_map().find(account_id);
  if (it == cs->bank_.account_map().end()) {  // account doesn't exist, create a new account
    Account account(account_id, 0);
    std::pair<string,Account> pair_input (account_id, account);
    auto pair = cs->bank_.account_map().insert(pair_input); 
    assert(pair.second); 
  }
  else {  // get balance of the account
    balance = (*it).second.balance();
  }
  return balance;
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<std::string>(), "specify config file path")(
        "second,s", "I'm second chain server");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    cs = std::unique_ptr<ChainServer>(new ChainServer("bank1"));

    int server_port;
    if (vm.count("second")) {
      is_tail = true;
      is_head = false;
      server_port = 50002;
    } else {
      //is_tail = false;
      //is_head = true;
      cs->set_ishead(true);
      cs->set_istail(true);
      server_port = 50001;
    }

    ChainServerUDPLoop udp_loop(server_port);
    ChainServerTCPLoop tcp_loop(server_port);
    std::thread udp_thread(udp_loop);
    std::thread tcp_thread(tcp_loop);
    udp_thread.join();
    tcp_thread.join();

    /*
    TODO read config file
    if (vm.count("config-file")) {
      ifstream ifs(vm["config-file"].as<string>());
      if (!ifs) {
        cerr << "open '" << vm["config-file"].as<string>() << "' failed"
             << endl;
        return 1;
      }
      string line;
      getline(ifs, line);
      cout << line;
    } else {
      cerr << "config-file was not set." << endl;
      return 1;
    }
    */
  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

