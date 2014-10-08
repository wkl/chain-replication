/**
 * @file  server.h
 * @brief
 */

#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <deque> 

#include "common.h"
#include "message.h"
#include "bank.h"
#include "account.h"

using std::string;
using std::unordered_map;
using std::deque;
using std::cout;
using std::endl;

class ChainServer {
 public:
  enum class CheckRequest { Inconsistent, Processed, NewReq };

  ChainServer() {};
  ChainServer(string bank_id) : bank_id_(bank_id) {
    cout << "bank " << bank_id << " initialized" << endl;
    bank_update_seq_ = 0;
  }

  void receive_request(proto::Request *req);

  // test function, to be removed
  void forward_request(proto::Request *req);
  // test function, to be removed
  void reply(const proto::Request &req);
  void handle_query(const proto::Request& req);
  void single_handle_update(proto::Request* request);
  float get_balance(string account_id);
  proto::Reply get_update_req_result(proto::Request* request);
  ChainServer::CheckRequest check_update_request(proto::Request* request);
  Account& get_or_create_account(proto::Request* request, bool* ifexisted_account);
  bool check_req_consistency(const proto::Request& req1, const proto::Request& req2);

  // getter/setter
  void set_ishead(bool ishead) { ishead_ = ishead; };
  bool ishead() { return ishead_; };
  void set_istail(bool istail) { istail_ = istail; };
  bool istail() { return istail_; };

 private:
  string bank_id_;
  Bank bank_;
  bool ishead_;
  bool istail_;
  //bool extending_chain_;
  unordered_map<string, proto::Request> processed_update_map_;	// <req_id, request>
  deque<proto::Request> sent_req_list_;
  int bank_update_seq_;
  proto::Address local_addr_;
  proto::Address pre_server_addr_;
  proto::Address succ_server_addr_;
  unordered_map<string, proto::Address> bank_head_list_;  // <bankid, bankaddr>
};

class ChainServerUDPLoop : public UDPLoop {
  using UDPLoop::UDPLoop;  // inherit constructor
  void handle_msg(proto::Message &msg);
};

class ChainServerTCPLoop : public TCPLoop {
  using TCPLoop::TCPLoop;
  void handle_msg(proto::Message &msg);
};

#endif

