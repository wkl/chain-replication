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
using std::cerr;

class ChainServer {
 public:
  enum class UpdateBalanceOutcome { Success, InsufficientFunds };

  ChainServer() { bank_update_seq_ = 0; };
  ChainServer(string bank_id) : bank_id_(bank_id) { bank_update_seq_ = 0; }

  void receive_request(proto::Request* req);
  void forward_request(const proto::Request& req);
  void reply_to_client(const proto::Request& req);
  void receive_ack(proto::Acknowledge* ack);
  void sendback_ack(const proto::Acknowledge& ack);

  void handle_query(proto::Request* req);
  void head_handle_update(proto::Request* req);
  void single_handle_update(proto::Request* req);
  void tail_handle_update(proto::Request* req);
  void internal_handle_update(proto::Request* req);
  float get_balance(string account_id);
  void get_update_req_result(proto::Request* req);
  proto::Request_CheckRequest check_update_request(const proto::Request& req,
                                                   proto::Reply* reply);
  Account& get_or_create_account(const proto::Request& req, bool& new_account);
  bool req_consistent(const proto::Request& req1, const proto::Request& req2);
  ChainServer::UpdateBalanceOutcome update_balance(const proto::Request& req);
  void insert_processed_list(const proto::Request& req);
  void insert_sent_req_list(const proto::Request& req);
  void pop_sent_req_list(string req_id);
  void write_log_reply(const proto::Reply& reply);

  // getter/setter
  void set_bank_id(string bank_id) { bank_id_ = bank_id; };
  string bank_id() { return bank_id_; };
  void set_bank(Bank bank) { bank_ = bank; };
  Bank& bank() { return bank_; };
  void set_ishead(bool ishead) { ishead_ = ishead; };
  bool ishead() { return ishead_; };
  void set_istail(bool istail) { istail_ = istail; };
  bool istail() { return istail_; };
  void set_pre_server_addr(proto::Address pre_server_addr) {
    pre_server_addr_ = pre_server_addr;
  };
  proto::Address& pre_server_addr() { return pre_server_addr_; };
  void set_succ_server_addr(proto::Address succ_server_addr) {
    succ_server_addr_ = succ_server_addr;
  };
  proto::Address& succ_server_addr() { return succ_server_addr_; };
  void set_local_addr(proto::Address local_addr) { local_addr_ = local_addr; };
  proto::Address& local_addr() { return local_addr_; };

 private:
  string bank_id_;
  Bank bank_;
  bool ishead_;
  bool istail_;
  // bool extending_chain_;
  // <"reqid_accountid", request>
  unordered_map<string, proto::Request> processed_map_;
  deque<proto::Request> sent_req_list_;
  unsigned int bank_update_seq_;
  proto::Address local_addr_;
  proto::Address pre_server_addr_;
  proto::Address succ_server_addr_;
  unordered_map<string, proto::Address> bank_head_list_;  // <bankid, bankaddr>
};

class ChainServerUDPLoop : public UDPLoop {
  using UDPLoop::UDPLoop;  // inherit constructor
  void handle_msg(proto::Message& msg, proto::Address& from_addr);
};

class ChainServerTCPLoop : public TCPLoop {
  using TCPLoop::TCPLoop;
  void handle_msg(proto::Message& msg, proto::Address& from_addr);
};

// global
int read_config_server(string dir, string bankid, int chainno);
bool get_server_json_with_chainno(Json::Value server_list_json,
                                  Json::Value& result_server_json, int chainno);

#endif

