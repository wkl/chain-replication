#include <list>
#include <mutex>
#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<ChainServer> cs;
proto::Address master_addr;
int heartbeat_interval;	// in sec
int send_req_seq_extend = 0;
std::mutex mutex_lock;

void ChainServerUDPLoop::handle_msg(proto::Message& msg,
                                    proto::Address& from_addr) {
  rec_msg_seq++;
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      LOG(INFO) << "Server received udp message from " << from_addr.ip() << ":"
                << from_addr.port() << endl 
                << msg.ShortDebugString() << endl << endl;
      cs->if_server_crash();
      cs->receive_request(msg.mutable_request());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

void ChainServerTCPLoop::handle_msg(proto::Message& msg,
                                    proto::Address& from_addr) {
  rec_msg_seq++;
  LOG(INFO) << "Server received tcp message from " << from_addr.ip() << ":"
            << from_addr.port() << endl
            << msg.ShortDebugString() << endl << endl;
  cs->if_server_crash();
  cs->set_internal_crashing(false); 
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    case proto::Message::ACKNOWLEDGE:
      assert(msg.has_ack());
      cs->receive_ack(msg.mutable_ack());
      break;
    case proto::Message::TO_BE_HEAD:
      cs->to_be_head();
      break;
    case proto::Message::NEW_HEAD:
      assert(msg.has_notify());
      assert(cs->istail());
      // to be continued in transfer phase
      break;
    case proto::Message::TO_BE_TAIL:
      cs->to_be_tail();
      break;
    case proto::Message::NEW_PRE_SERVER:
      assert(msg.has_addr());
      cs->receive_new_preserver(msg.addr());
      break;
    case proto::Message::NEW_SUCC_SERVER:
      assert(msg.has_reqseq());
      cs->receive_new_succserver(msg.reqseq());
      break;      
    case proto::Message::EXTEND_SERVER:
      assert(msg.has_addr());
      cs->receive_extend_server(msg.addr());
      break;
    case proto::Message::EXTEND_MSG:
      assert(msg.has_extendmsg());
      cs->receive_extend_msg(msg.extendmsg());
      break;
    case proto::Message::EXTEND_FAIL:
      cs->extending_server_fail();
      break;      
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

// Notified to be head server
void ChainServer::to_be_head() {
  LOG(INFO) << "Notified to be head server" << endl << endl;
  ishead_ = true;
  LOG(INFO) << "Ready to be head server" << endl << endl;
}

// Notified to be tail server
void ChainServer::to_be_tail() {
  LOG(INFO) << "Notified to be tail server" << endl << endl;
  istail_ = true;
  bool ack_flag = false;
  while (!sent_list_.empty()) { // not reply to client, client will regard as reply lost
    proto::Request req = sent_list_.front();
    insert_processed_list(req);
    pop_sent_list();
    ack_flag = true;
  }
  if (!ishead_ && ack_flag) {
    proto::Acknowledge ack;
    ack.set_bank_update_seq(bank_update_seq_);
    sendback_ack(ack);
  }  
  LOG(INFO) << "Ready to be tail server" << endl << endl;
}

// Notified a new pre server
void ChainServer::receive_new_preserver(const proto::Address& pre_addr) {
  LOG(INFO) << "Notified pre server address is changed to " 
            << pre_addr.ip() << ":" << pre_addr.port() 
            << endl << endl;
  internal_crashing_ = true;
  pre_server_addr_ = pre_addr;
  proto::Reqseq req_seq;
  req_seq.set_bank_update_seq(bank_update_seq_);
  req_seq.set_bank_id(bank_id_);
  auto *pre_addr_copy = new proto::Address;
  pre_addr_copy->CopyFrom(pre_server_addr_);
  req_seq.set_allocated_pre_addr(pre_addr_copy);
  auto *local_addr_copy = new proto::Address;
  local_addr_copy->CopyFrom(local_addr_);
  req_seq.set_allocated_succ_addr(local_addr_copy);
  send_msg_tcp(master_addr, proto::Message::REQ_SEQ, req_seq);
  LOG(INFO) << "Send sequence number of the last update request to the master: " 
            << bank_update_seq_ << endl << endl;
}

// Notified a new succ server
void ChainServer::receive_new_succserver(const proto::Reqseq& req_seq) {
  LOG(INFO) << "Notified succ server address is changed to " 
            << req_seq.succ_addr().ip() << ":" << req_seq.succ_addr().port() 
            << ", should send update requests in the sentlist to S+ server with sequence number>"
            << req_seq.bank_update_seq() 
            << endl << endl;
  internal_crashing_ = true;
  if_server_crash();
  succ_server_addr_ = req_seq.succ_addr();
  for (auto& it : sent_list_) {
    if (it.bank_update_seq() > req_seq.bank_update_seq())
      forward_request(it);
  }
  LOG(INFO) << "Finish sending update requests in the sentlist to S+ server with sequence number>" 
            << req_seq.bank_update_seq() 
            << endl << endl; 
  internal_crashing_ = false;          
}

// Notified a extend server
void ChainServer::receive_extend_server(const proto::Address& extend_addr) {
  extending_chain_ = true;
  finish_sending_hist_ = false;
  send_req_seq_extend = 0;
  succ_server_addr_ = extend_addr;
  LOG(INFO) << "Notified a new server " 
            << extend_addr.ip() << ":" << extend_addr.port() 
            << " want to join" 
            << endl << endl;
  std::thread send_req_to_extend_server_thread(send_req_to_extend_server);
  send_req_to_extend_server_thread.detach();
}

// Current Tail send request(account, processed_list, sent_list, fin) to extended server
void send_req_to_extend_server() {
  // deep copy cs->bank().account_map(), cs->processed_list(), mutex lock
  std::list<proto::Account*> account_map_copy;
  std::list<proto::Request*> processed_map_copy;
  {
    std::lock_guard<std::mutex> lock(mutex_lock);
    for (auto& it : cs->bank().account_map()) {
      auto *account = new proto::Account;
      account->set_account_id(it.second.accountid());
      account->set_balance(it.second.balance());
      account_map_copy.push_back(account);
    }
    for (auto& it : cs->processed_map()) {
      auto *req = new proto::Request;
      req->CopyFrom(it.second);  
      processed_map_copy.push_back(req);
    }
  }
  // send start msg
  proto::ExtendMsg extend_msg_start;
  extend_msg_start.set_type(proto::ExtendMsg::START);
  auto *local_addr = new proto::Address;
  local_addr->CopyFrom(cs->local_addr());
  extend_msg_start.set_allocated_server_addr(local_addr);
  bool send_res = send_msg_tcp(cs->succ_server_addr(), proto::Message::EXTEND_MSG, extend_msg_start);
  if (!send_res) {
    LOG(INFO) << "Server can't get connect to new extended server, give up"
              << endl << endl;
    return;
  }
  else {
    LOG(INFO) << "Server sent tcp message to " << cs->succ_server_addr().ip() << ":"
              << cs->succ_server_addr().port()
              << endl << extend_msg_start.ShortDebugString() << endl << endl;  
  }
  send_req_seq_extend++;
  cs->if_server_crash();
  std::this_thread::sleep_for(std::chrono::seconds(cs->extend_send_delay()));
  // send account info
  for (auto& it : account_map_copy) {
    proto::ExtendMsg extend_msg;
    extend_msg.set_type(proto::ExtendMsg::ACCOUNT);
    extend_msg.set_allocated_account(it);
    send_res = send_msg_tcp(cs->succ_server_addr(), proto::Message::EXTEND_MSG, extend_msg);
    if (!send_res) {
      LOG(INFO) << "Server can't get connect to new extended server, give up"
                << endl << endl;
      return;
    }
    else {
      LOG(INFO) << "Server sent tcp message to " << cs->succ_server_addr().ip() << ":"
                << cs->succ_server_addr().port()
                << endl << extend_msg.ShortDebugString() << endl << endl;  
    } 
    send_req_seq_extend++;
    cs->if_server_crash();   
    std::this_thread::sleep_for(std::chrono::seconds(cs->extend_send_delay()));
  }
  // send request in the processed_list
  for (auto& it : processed_map_copy) {
    proto::ExtendMsg extend_msg;
    extend_msg.set_type(proto::ExtendMsg::HISTORY);
    extend_msg.set_allocated_request(it);
    send_res = send_msg_tcp(cs->succ_server_addr(), proto::Message::EXTEND_MSG, extend_msg);
    if (!send_res) {
      LOG(INFO) << "Server can't get connect to new extended server, give up"
                << endl << endl;
      return;
    }    
    else {
      LOG(INFO) << "Server sent tcp message to " << cs->succ_server_addr().ip() << ":"
                << cs->succ_server_addr().port()
                << endl << extend_msg.ShortDebugString() << endl << endl;  
    }   
    send_req_seq_extend++;
    cs->if_server_crash();   
    std::this_thread::sleep_for(std::chrono::seconds(cs->extend_send_delay()));
  }
  cs->set_finish_sending_hist(true);
  // send request in the sent_list, lock main thread of tail dealing new request
  {
    std::lock_guard<std::mutex> lock(mutex_lock);
    for (auto& it : cs->sent_list()) {
      auto *req = new proto::Request;
      req->CopyFrom(it); 
      proto::ExtendMsg extend_msg;
      extend_msg.set_type(proto::ExtendMsg::SENT);
      extend_msg.set_allocated_request(req);
      send_res = send_msg_tcp(cs->succ_server_addr(), proto::Message::EXTEND_MSG, extend_msg);
      if (!send_res) {
        LOG(INFO) << "Server can't get connect to new extended server, give up"
                  << endl << endl;
        return;
      } 
      else {
        LOG(INFO) << "Server sent tcp message to " << cs->succ_server_addr().ip() << ":"
                  << cs->succ_server_addr().port()
                  << endl << extend_msg.ShortDebugString() << endl << endl;  
      }     
      send_req_seq_extend++;
      cs->if_server_crash();     
    }
  }
  // send fin to extend server
  proto::ExtendMsg extend_msg_fin;
  extend_msg_fin.set_type(proto::ExtendMsg::FIN);
  send_res = send_msg_tcp(cs->succ_server_addr(), proto::Message::EXTEND_MSG, extend_msg_fin);
  if (!send_res) {
    LOG(INFO) << "Server can't get connect to new extended server, give up"
              << endl << endl;
    return;
  }  
  else {
    LOG(INFO) << "Server sent tcp message to " << cs->succ_server_addr().ip() << ":"
              << cs->succ_server_addr().port()
              << endl << extend_msg_fin.ShortDebugString() << endl << endl;  
  }  
  // stop acting as a tail server
  LOG(INFO) << "Stop acting as the tail server" 
            << endl << endl;
  cs->set_extending_chain(false);
  cs->set_finish_sending_hist(false);
  cs->set_istail(false);
}

// extend server receive request(account, processed_list, sent_list, fin) from current tail
void ChainServer::receive_extend_msg(const proto::ExtendMsg& extend_msg) {
  if (extend_msg.type() == proto::ExtendMsg::START) {
    // clear processed_list, bank_update_seq, pre_server_addr, bank.account
    processed_map_.clear();
    bank_.account_map().clear();
    bank_update_seq_ = 0;
    pre_server_addr_ = extend_msg.server_addr();
    LOG(INFO) << "Server begins to receive history records from current tail server " 
              << cs->pre_server_addr().ip() << ":" << cs->pre_server_addr().port()
              << ", clear previous records if there's any"
              << endl << endl;
    return;
  }
  if (extend_msg.type() == proto::ExtendMsg::ACCOUNT) {
    assert(extend_msg.has_account());
    create_account(extend_msg.account());
    return;
  }
  if (extend_msg.type() == proto::ExtendMsg::HISTORY) {
    assert(extend_msg.has_request());
    // update bank_update_seq_
    if (extend_msg.request().bank_update_seq() > bank_update_seq_)
      bank_update_seq_ = extend_msg.request().bank_update_seq();
    insert_processed_list(extend_msg.request());
    return;
  }
  if (extend_msg.type() == proto::ExtendMsg::SENT) {
    assert(extend_msg.has_request());
    // update bank_update_seq_
    if (extend_msg.request().bank_update_seq() > bank_update_seq_)
      bank_update_seq_ = extend_msg.request().bank_update_seq();    
    proto::Request req = extend_msg.request();
    update_request_reply(&req);
    write_log_reply(req.reply());

    if (req.check_result() == proto::Request::NEWREQ)
      insert_processed_list(req);   
      return;
  }
  if (extend_msg.type() == proto::ExtendMsg::FIN) {
    istail_ = true;
    proto::ExtendFinish extend_finish;
    extend_finish.set_bank_id(bank_id_);
    auto *local_addr = new proto::Address;
    local_addr->CopyFrom(local_addr_);
    extend_finish.set_allocated_server_addr(local_addr);
    send_msg_tcp(master_addr, proto::Message::EXTEND_FINISH, extend_finish);
    LOG(INFO) << "Server begin to act as the tail server, notify master server"
              << endl << endl;    
    return;
  }
}

// current tail server is notified new extending server is crashed
void ChainServer::extending_server_fail() {
  cs->set_extending_chain(false);
  cs->set_finish_sending_hist(false);
  cs->sent_list().clear(); 
  LOG(INFO) << "Notified that the new extending server is crashed, sent list is cleared"
            << endl << endl;
}

// Server crash scenario
void ChainServer::if_server_crash() {
  switch (fail_scenario_) {
    case ChainServer::FailScenario::None:
      break;
    case ChainServer::FailScenario::FailAfterSend:
      if (send_msg_seq == fail_seq_) {
        LOG(INFO) << "Server crashed after sending " 
                  << send_msg_seq << " messages." 
                  << endl << endl;
        exit(0);
      }
      break;
    case ChainServer::FailScenario::FailAfterRecv:
      if (rec_msg_seq == fail_seq_) {
        LOG(INFO) << "Server crashed after receiving " 
                  << rec_msg_seq << " messages." 
                  << endl << endl;
        exit(0);
      }
      break;
    case ChainServer::FailScenario::FailAfterIntervalFail:
      if (internal_crashing_) {
        LOG(INFO) << "Server crashed immediately after interval server crashed"
                  << endl << endl;
        exit(0);
      }
      break;
    case ChainServer::FailScenario::FailAfterSendInExtend:
      if (send_req_seq_extend == fail_seq_) {
        LOG(INFO) << "Server (current tail) crashed during chain extension"
                  << endl << endl;
        exit(0);
      }
    default:
      break;
  }
}


// server forward request to its succ server
void ChainServer::forward_request(const proto::Request& req) {
  if (send_msg_tcp(succ_server_addr_, proto::Message::REQUEST, req)) {  // send tcp message successfully
    send_msg_seq++;
    LOG(INFO) << "Server sent tcp message to " << succ_server_addr_.ip() << ":"
              << succ_server_addr_.port() << endl 
              << req.ShortDebugString() << endl << endl;
    if_server_crash();  // for FailAfterSend & FailAfterSendInExtend scenario
  }
}

// tail/single server reply to client
void ChainServer::reply_to_client(const proto::Request& req) {
  proto::Reply reply = req.reply();
  proto::Address client;
  client.set_ip(req.client_addr().ip());
  client.set_port(req.client_addr().port());
  send_msg_udp(local_addr_, client, proto::Message::REPLY, reply);
  send_msg_seq++;
  LOG(INFO) << "Server sent udp message to " << client.ip() << ":"
            << client.port() << endl
            << reply.ShortDebugString() << endl << endl;
  if_server_crash();  // for FailAfterSend & FailAfterSendInExtend scenario
}

// server send ack to pre server
void ChainServer::sendback_ack(const proto::Acknowledge& ack) {
  if (send_msg_tcp(pre_server_addr_, proto::Message::ACKNOWLEDGE, ack)) { // send tcp message successfully
    send_msg_seq++;
    LOG(INFO) << "Server sent tcp message to " << pre_server_addr_.ip() << ":"
              << pre_server_addr_.port()
              << endl << ack.ShortDebugString() << endl << endl;
    if_server_crash();  // for FailAfterSend & FailAfterSendInExtend scenario
  }
}

// server receive ack
void ChainServer::receive_ack(proto::Acknowledge* ack) {
  while (!sent_list_.empty() &&
         sent_list_.front().bank_update_seq() <= ack->bank_update_seq()) {
    proto::Request req = sent_list_.front();
    insert_processed_list(req);
    pop_sent_list();
  }
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
  std::lock_guard<std::mutex> lock(mutex_lock);
  if (ishead_ && !istail_) {
    bank_update_seq_++;  // sequence of update request
    req->set_bank_update_seq(bank_update_seq_);
    head_handle_update(req);
    return;
  }
  // when tail is sending request in the sent_list in the extending stage, should wait here?
  
  // only one server
  if (ishead_ && istail_) {
    bank_update_seq_++;  // sequence of update request
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
  proto::Reply* reply = new proto::Reply;

  reply->set_outcome(proto::Reply::PROCESSED);
  reply->set_req_id(req->req_id());
  reply->set_balance(get_balance(req->account_id()));
  reply->set_account_id(req->account_id());
  req->set_allocated_reply(reply);

  reply_to_client(*req);
}

// head server handle update request
void ChainServer::head_handle_update(proto::Request* req) {
  update_request_reply(req);
  write_log_reply(req->reply());
  insert_sent_list(*req);

  forward_request(*req);
}

// single server handle update request
void ChainServer::single_handle_update(proto::Request* req) {
  update_request_reply(req);
  write_log_reply(req->reply());

  if (req->check_result() == proto::Request::NEWREQ)
    insert_processed_list(*req);
  if (extending_chain_)
    insert_sent_list(*req);
    
  reply_to_client(*req);
}

// tail server handle update request
void ChainServer::tail_handle_update(proto::Request* req) {
  update_request_reply(req);
  write_log_reply(req->reply());

  if (req->check_result() == proto::Request::NEWREQ)
    insert_processed_list(*req);
  if (extending_chain_)
    insert_sent_list(*req);

  if (req->type() != proto::Request::TRANSFERTO) 
    reply_to_client(*req);

  proto::Acknowledge ack;
  ack.set_bank_update_seq(req->bank_update_seq());
  sendback_ack(ack);
}

// interval server handle update request
void ChainServer::internal_handle_update(proto::Request* req) {
  update_request_reply(req);
  write_log_reply(req->reply());
  insert_sent_list(*req);

  forward_request(*req);
}

// used in all the xxxx_handle_update
void ChainServer::update_request_reply(proto::Request* req) {
  proto::Reply* reply = new proto::Reply;
  reply->set_req_id(req->req_id());
  reply->set_account_id(req->account_id());
  proto::Request_CheckRequest check_result =
      check_update_request(*req, &(*reply));
  req->set_check_result(check_result);
  float balance = 0;
  switch (check_result) {
    case proto::Request::INCONSISTENT:
      reply->set_outcome(proto::Reply::INCONSISTENT_WITH_HISTORY);
      balance = get_balance(req->account_id());
      break;
    case proto::Request::PROCESSED:
      break;
    case proto::Request::NEWREQ:
      ChainServer::UpdateBalanceOutcome update_result = update_balance(*req);
      if (update_result ==
          ChainServer::UpdateBalanceOutcome::InsufficientFunds) {
        reply->set_outcome(proto::Reply::INSUFFICIENT_FUNDS);
        balance = get_balance(req->account_id());
      } else {
        reply->set_outcome(proto::Reply::PROCESSED);
        balance = get_balance(req->account_id());
      }
      break;
  }
  if (check_result != proto::Request::PROCESSED) reply->set_balance(balance);
  req->set_allocated_reply(reply);
}

// used in update_request_reply
proto::Request_CheckRequest ChainServer::check_update_request(
    const proto::Request& req, proto::Reply* reply) {
  bool new_account;
  get_or_create_account(req, new_account);
  if (new_account) return proto::Request::NEWREQ;

  auto it = processed_map_.find(req.req_id() + "_" + req.account_id());
  if (it != processed_map_.end()) {
    // request exists in processed_map_
    if (req_consistent(req, it->second)) {
      *reply = (it->second).reply();
      return proto::Request::PROCESSED;
    } else {
      return proto::Request::INCONSISTENT;
    }
  }

  // doesn't exist in processed_map_
  for (auto it = sent_list_.begin(); it != sent_list_.end(); ++it) {
    if (req.req_id() == it->req_id()) {
      // request exists in sent_list_
      if (req_consistent(req, *it)) {
        *reply = (*it).reply();
        return proto::Request::PROCESSED;
      } else {
        return proto::Request::INCONSISTENT;
      }
    }
  }

  // request doesn't exist in sent_list_ either
  return proto::Request::NEWREQ;
}

// get or create account object
Account& ChainServer::get_or_create_account(const proto::Request& req,
                                            bool& new_account) {
  string account_id = req.account_id();
  auto it = bank_.account_map().find(account_id);
  if (it == bank_.account_map().end()) {
    // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it_insert =
        bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it_insert.second);
    new_account = true;
    return (it_insert.first)->second;
  } else {  // get the account
    new_account = false;
    return (*it).second;
  }
}

// create account object
bool ChainServer::create_account(const proto::Account& account) {
  Account accountObject(account.account_id(), account.balance());
  auto it_insert =
        bank_.account_map().insert(std::make_pair(accountObject.accountid(), accountObject));
  assert(it_insert.second);
  LOG(INFO) << "Server create account with account_id=" 
            << account.account_id() 
            << ", balance=" << account.balance() 
            << endl << endl;
  return true;
}

// check if the contents of 2 requests with same req_id are consistent
bool ChainServer::req_consistent(const proto::Request& req1,
                                 const proto::Request& req2) {
  return ((req1.type() == req2.type()) &&
          //(req1.account_id() == req2.account_id()) &&
          (req1.bank_id() == req2.bank_id()) &&
          (req1.amount() == req2.amount()) &&
          (req1.dest_bank_id() == req2.dest_bank_id()) &&
          (req1.dest_account_id() == req2.dest_account_id()));
}

// used in handle_query
float ChainServer::get_balance(string account_id) {
  float balance = 0;
  auto it = bank_.account_map().find(account_id);
  if (it == bank_.account_map().end()) {
    // account doesn't exist, create a new account
    Account account(account_id, 0);
    auto it2 = bank_.account_map().insert(std::make_pair(account_id, account));
    assert(it2.second);
  } else {  // get balance of the account
    balance = (*it).second.balance();
  }
  return balance;
}

// used in check_update_request
ChainServer::UpdateBalanceOutcome ChainServer::update_balance(
    const proto::Request& req) {
  bool new_account;
  Account& account = get_or_create_account(req, new_account);
  if (req.type() == proto::Request::WITHDRAW ||
      req.type() == proto::Request::TRANSFER) {
    if (account.balance() < req.amount()) {
      return ChainServer::UpdateBalanceOutcome::InsufficientFunds;
    } else {
      float pre_balance = account.balance();
      account.set_balance(account.balance() - req.amount());
      LOG(INFO) << "Balance of account " << account.accountid() 
                << " changes from " << pre_balance 
                << " to " << account.balance() 
                << endl << endl;
      return ChainServer::UpdateBalanceOutcome::Success;
    }
  } else {
    float pre_balance = account.balance();
    account.set_balance(account.balance() + req.amount());
    LOG(INFO) << "Balance of account " << account.accountid() 
              << " changes from " << pre_balance 
              << " to " << account.balance() 
              << endl << endl;
    return ChainServer::UpdateBalanceOutcome::Success;
  }
}

// used in tail_handle_update(req), single_handle_update(req) and receive_extend_msg(extend_msg)
void ChainServer::insert_processed_list(const proto::Request& req) {
  auto it = processed_map_.find(req.req_id() + "_" + req.account_id());
  if (it == processed_map_.end()) {  // doesn't exist in processed list
    auto it_insert = processed_map_.insert(
        std::make_pair(req.req_id() + "_" + req.account_id(), req));
    assert(it_insert.second);
    LOG(INFO) << "Server added request req_id=" << req.req_id()
              << ", bank_update_seq=" << req.bank_update_seq()
              << " to processed list" << endl << endl;
  }
}

// used in head_handle_update(req) and interval_handle_update(req)
void ChainServer::insert_sent_list(const proto::Request& req) {
  sent_list_.push_back(req);  // insert at the end of deque
  LOG(INFO) << "Server added request req_id=" << req.req_id()
            << ", bank_update_seq=" << req.bank_update_seq()
            << " to sent list" << endl << endl;
}

// used in receive_ack(ack)
void ChainServer::pop_sent_list() {
  proto::Request req = sent_list_.front();
  LOG(INFO) << "Server removed request req_id=" << req.req_id()
            << ", bank_update_seq=" << req.bank_update_seq()
            << " from sent list" << endl << endl;
  sent_list_.pop_front();
}

// write processed request result to log
void ChainServer::write_log_reply(const proto::Reply& reply) {
  string outcome;
  switch (reply.outcome()) {
    case proto::Reply::PROCESSED:
      outcome = "PROCESSED";
      break;
    case proto::Reply::INCONSISTENT_WITH_HISTORY:
      outcome = "INCONSISTENT_WITH_HISTORY";
      break;
    case proto::Reply::INSUFFICIENT_FUNDS:
      outcome = "INSUFFICIENT_FUNDS";
      break;
  }
  LOG(INFO) << "Server processed request req_id=" << reply.req_id()
            << ", outcome=" << outcome << ", balance=" << reply.balance()
            << endl << endl;
}

// TODO delayed server also delay heartbeat
void heartbeat() {
  proto::Heartbeat hb;
  hb.set_bank_id(cs->bank_id());
  auto *server_addr = new proto::Address();
  server_addr->CopyFrom(cs->local_addr());
  hb.set_allocated_server_addr(server_addr);
  
  if (cs->start_delay() > 0) {  // for extended servers
    std::this_thread::sleep_for(std::chrono::seconds(cs->start_delay()));
    LOG(INFO) << "Extended Server starts up after " 
              << cs->start_delay() << " sec delay, " 
              << "send join request to master "
              << master_addr.ip() << ":"
              << master_addr.port()
              << endl << endl;
    proto::Join join;
    join.set_bank_id(cs->bank_id());
    auto *local_addr_copy = new proto::Address;
    local_addr_copy->CopyFrom(cs->local_addr());
    join.set_allocated_server_addr(local_addr_copy);
    send_msg_tcp(master_addr, proto::Message::JOIN, join);    
  }

  for (;;) {
    send_msg_udp(cs->local_addr(), master_addr, proto::Message::HEARTBEAT, hb);
    std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval));
  }
}

// read configuration file for a server
int read_config_server(string dir, string bankid, int chainno) {
  Json::Reader reader;
  Json::Value root;
  std::ifstream ifs;

  ifs.open(dir, std::ios::binary);
  if (!ifs.is_open()) {
    LOG(ERROR) << "Fail to open the config file: " << dir << endl << endl;
    return 1;
  }

  if (!reader.parse(ifs, root)) {
    ifs.close();
    LOG(ERROR) << "Fail to parse the config file: " << dir << endl << endl;
    return 1;
  }

  srand((unsigned)time(0));
  bool find_bank = false;
  Json::Value bank_list_json = root[JSON_BANKS];
  for (unsigned int i = 0; i < bank_list_json.size(); i++) {
    if (bank_list_json[i][JSON_BANKID].asString() == bankid) {
      find_bank = true;
      Json::Value bank_json = bank_list_json[i];
      Json::Value server_list_json = bank_json[JSON_SERVERS];
      Json::Value extend_server_list_json = bank_json[JSON_EXTEND_SERVERS];
      Json::Value server_json;
      bool find_res = get_server_json_with_chainno(server_list_json,
						   extend_server_list_json,
                                                   server_json, chainno);
      if (!find_res) {
        ifs.close();
        LOG(ERROR) << "The chainno is not in the config file: " << dir << endl
                     << endl;
        return 1;
      }
      cs->set_bank_id(bankid);
      Bank bank(bankid);
      cs->set_bank(bank);
      cs->set_start_delay(server_json[JSON_START_DEALY].asInt());
      heartbeat_interval = root[JSON_CONFIG][JSON_SERVER_REPORT_INTERVAL].asInt();
      if (!root[JSON_CONFIG].isMember(JSON_EXTEND_SEND_DELAY))
        cs->set_extend_send_delay(0);
      else {
        cs->set_extend_send_delay(root[JSON_CONFIG][JSON_EXTEND_SEND_DELAY].asInt());
      }
      string fail_scenario = server_json[JSON_FAIL_SCENARIO].asString();
      if (fail_scenario == JSON_NONE)
        cs->set_fail_scenario(ChainServer::FailScenario::None);
      else if (fail_scenario == JSON_FAIL_AFTER_SEND)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterSend);
      else if (fail_scenario == JSON_FAIL_AFTER_RECV)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterRecv); 
      else if (fail_scenario == JSON_FAIL_AFTER_SEND_IN_EXTEND)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterSendInExtend);
      else if (fail_scenario == JSON_FAIL_AFTER_RECV_IN_EXTEND)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterRecvInExtend); 
      else if (fail_scenario == JSON_FAIL_AFTER_INTERVAL_FAIL)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterIntervalFail);
      else if (fail_scenario == JSON_FAIL_AFTER_SEND_IN_EXTEND)
        cs->set_fail_scenario(ChainServer::FailScenario::FailAfterSendInExtend);
      if (!server_json.isMember(JSON_FAIL_SEQ))
        cs->set_fail_seq(-1);
      else {
        string fail_seq = server_json[JSON_FAIL_SEQ].asString();
        if (fail_seq == JSON_RANDOM) {
          cs->set_fail_seq((rand() % 7) + 2);  // use some reasonable random data
          LOG(INFO) << "Randomly generate server fail_seq=" << cs->fail_seq()
                    << endl << endl;
        }
        else
          cs->set_fail_seq(std::atoi(fail_seq.c_str()));
      }
      cs->set_internal_crashing(false);
      master_addr.set_ip(root[JSON_MASTER][JSON_IP].asString());
      master_addr.set_port(root[JSON_MASTER][JSON_PORT].asInt());

      cs->set_ishead(false);
      cs->set_istail(false);
      if (chainno == 1) {  
        cs->set_ishead(true);
      }
      if (chainno == (int)server_list_json.size()) {
        cs->set_istail(true);
      }
      proto::Address local_addr;  // local address
      local_addr.set_ip(server_json[JSON_IP].asString());
      local_addr.set_port(server_json[JSON_PORT].asInt());
      cs->set_local_addr(local_addr);
      if (!cs->ishead() && cs->start_delay() == 0) {  // for alive server: pre_server address
        Json::Value pre_server_json;
        bool find_res = get_alive_server_json_with_chainno(
             server_list_json, pre_server_json, chainno - 1);
        assert(find_res);
        proto::Address pre_server;
        pre_server.set_ip(pre_server_json[JSON_IP].asString());
        pre_server.set_port(pre_server_json[JSON_PORT].asInt());
        cs->set_pre_server_addr(pre_server);
      }
      if (!cs->istail() && cs->start_delay() == 0) {  // for alive server: succ_server address
        Json::Value succ_server_json;
        bool find_res = get_alive_server_json_with_chainno(
             server_list_json, succ_server_json, chainno + 1);
        assert(find_res);
        proto::Address succ_server;
        succ_server.set_ip(succ_server_json[JSON_IP].asString());
        succ_server.set_port(succ_server_json[JSON_PORT].asInt());
        cs->set_succ_server_addr(succ_server);
      }
      break;
    }
  }
  if (!find_bank) {
    ifs.close();
    LOG(ERROR) << "The bankid specified is not involved in the config file: "
               << dir << endl << endl;
    return 1;
  }

  LOG_IF(INFO, (cs->ishead() && !cs->istail()))
      << "New server initialization: bankid=" << cs->bank_id()
      << ", head server"
      << ", local ip=" << cs->local_addr().ip()
      << ", local port=" << cs->local_addr().port()
      << ", succ_server ip=" << cs->succ_server_addr().ip()
      << ", succ_server port=" << cs->succ_server_addr().port() << endl << endl;
  LOG_IF(INFO, (!cs->ishead() && cs->istail()))
      << "New server initialization: bankid=" << cs->bank_id()
      << ", tail server"
      << ", local ip=" << cs->local_addr().ip()
      << ", local port=" << cs->local_addr().port()
      << ", pre_server ip=" << cs->pre_server_addr().ip()
      << ", pre_server port=" << cs->pre_server_addr().port() << endl << endl;
  LOG_IF(INFO, (!cs->ishead() && !cs->istail()))
      << "New server initialization: bankid=" << cs->bank_id()
      << ", internal server"
      << ", local ip=" << cs->local_addr().ip()
      << ", local port=" << cs->local_addr().port()
      << ", pre_server ip=" << cs->pre_server_addr().ip()
      << ", pre_server port=" << cs->pre_server_addr().port()
      << ", succ_server ip=" << cs->succ_server_addr().ip()
      << ", succ_server port=" << cs->succ_server_addr().port() << endl << endl;
  LOG_IF(INFO, (cs->ishead() && cs->istail()))
      << "New server initialization: bankid=" << cs->bank_id()
      << ", single server"
      << ", local ip=" << cs->local_addr().ip()
      << ", local port=" << cs->local_addr().port() << endl << endl;

  ifs.close();
  return 0;
}

bool get_server_json_with_chainno(Json::Value server_list_json,
				  Json::Value extend_server_list_json,
                                  Json::Value& result_server_json,
                                  int chainno) {
  for (unsigned int j = 0; j < server_list_json.size(); j++) {
    if (server_list_json[j][JSON_CHAINNO].asInt() == chainno) {
      result_server_json = server_list_json[j];
      return true;
    }
  }
  for (unsigned int j = 0; j < extend_server_list_json.size(); j++) {
    if (extend_server_list_json[j][JSON_CHAINNO].asInt() == chainno) {
      result_server_json = extend_server_list_json[j];
      return true;
    }
  }
  return false;
}

bool get_alive_server_json_with_chainno(Json::Value server_list_json,
                                  	Json::Value& result_server_json,
                                  	int chainno) {
  for (unsigned int j = 0; j < server_list_json.size(); j++) {
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
        "chain-no,n", po::value<int>(),
        "specify the server sequence in the chain");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << endl;
      return 0;
    }

    if (!vm.count("config-file") || !vm.count("bank-id") || !vm.count("chain-no")) {
        LOG(ERROR) << "Please input the config-file path, bankid and the server "
                    "sequence in the chain" << endl << endl;
        return 1;
    }

    FLAGS_logtostderr = true;
    if (vm.count("log-dir")) {
      FLAGS_log_dir = vm["log-dir"].as<string>();
      FLAGS_logtostderr = false;
      FLAGS_logbuflevel = -1;
      string log_name = "server_" + vm["bank-id"].as<string>() + "_No" +
                        std::to_string(vm["chain-no"].as<int>());
      google::InitGoogleLogging(log_name.c_str());
    } else {
      FLAGS_logbuflevel = -1;
      google::InitGoogleLogging(argv[0]);
    }

    LOG(INFO) << "Processing configuration file" << endl << endl;

    cs = std::unique_ptr<ChainServer>(new ChainServer());

    // read configuration file
    if (read_config_server(vm["config-file"].as<string>(),
                           vm["bank-id"].as<string>(), 
                           vm["chain-no"].as<int>()) != 0) {
      exit(1);
    }

    ChainServerUDPLoop udp_loop(cs->local_addr().port());
    ChainServerTCPLoop tcp_loop(cs->local_addr().port());
    std::thread udp_thread(udp_loop);
    std::thread tcp_thread(tcp_loop);

    std::thread heartbeat_thread(heartbeat);

    udp_thread.join();
    tcp_thread.join();
    heartbeat_thread.join();

  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}

