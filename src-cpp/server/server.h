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
#include <chrono>

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
  enum class FailScenario {None, FailAfterSend, FailAfterRecv, 
                           FailAfterSendInExtend, FailAfterRecvInExtend, 
                           FailAfterIntervalFail};

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
  void update_request_reply(proto::Request* req);
  proto::Request_CheckRequest check_update_request(const proto::Request& req,
                                                   proto::Reply* reply);
  Account& get_or_create_account(const proto::Request& req, bool& new_account);
  bool create_account(const proto::Account& account);
  bool req_consistent(const proto::Request& req1, const proto::Request& req2);
  ChainServer::UpdateBalanceOutcome update_balance(const proto::Request& req);
  void insert_processed_list(const proto::Request& req);
  void insert_sent_list(const proto::Request& req);
  void pop_sent_list();
  void write_log_reply(const proto::Reply& reply);
  
  void if_server_crash();
  void to_be_head();
  void to_be_tail();
  void receive_new_preserver(const proto::Address& pre_addr);
  void receive_new_succserver(const proto::Reqseq& req_seq);
  void receive_extend_server(const proto::Address& extend_addr);
  void receive_extend_msg(const proto::ExtendMsg& extend_msg);
  void extending_server_fail();

  // transfer
  proto::Address get_bank_head(const string& bankid) {
      auto it_head = bank_head_list_.find(bankid);
      assert(it_head != bank_head_list_.end());
      return it_head->second;
  }
  void handle_transfer(const proto::Request& req);
  void forward_transfer_to_downstream(const proto::Request& req);
  void receive_transfer_reply(proto::Reply *reply);
  void handle_new_head(const proto::Notify& notify);

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
  void set_start_delay(int start_delay) { start_delay_ = start_delay; };
  int start_delay() { return start_delay_; };
  void set_fail_scenario(FailScenario fail_scenario) { fail_scenario_ = fail_scenario; };
  FailScenario fail_scenario() { return fail_scenario_; };
  void set_fail_seq(int fail_seq) { fail_seq_ = fail_seq; };
  int fail_seq() { return fail_seq_; };
  bool internal_crashing() { return internal_crashing_; };
  void set_internal_crashing(bool internal_crashing) { internal_crashing_ = internal_crashing; };
  bool extending_chain() { return extending_chain_; };
  void set_extending_chain(bool extending_chain) { extending_chain_ = extending_chain; };
  bool finish_sending_hist() { return finish_sending_hist_; };
  void set_finish_sending_hist(bool finish_sending_hist) { finish_sending_hist_ = finish_sending_hist; };
  unordered_map<string, proto::Request>& processed_map() { return processed_map_;}
  deque<proto::Request>& sent_list() { return sent_list_; }
  void set_extend_send_delay(int extend_send_delay) { extend_send_delay_ = extend_send_delay; };
  int extend_send_delay() { return extend_send_delay_; };
  unordered_map<string, proto::Address>& bank_head_list() { return bank_head_list_; };

 private:
  string bank_id_;
  Bank bank_;
  bool ishead_;
  bool istail_;
  bool extending_chain_;  // for current tail server during extending chain
  bool finish_sending_hist_;  // for current tail server during extending chain
  bool internal_crashing_;
  int start_delay_;  // in sec
  int extend_send_delay_; // in sec
  FailScenario fail_scenario_;
  int fail_seq_;
  // <"reqid_accountid", request>
  unordered_map<string, proto::Request> processed_map_;
  deque<proto::Request> sent_list_;
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


int read_config_server(string dir, string bankid, int chainno);
bool get_server_json_with_chainno(Json::Value server_list_json,
                                  Json::Value extend_server_list_json,
                                  Json::Value& result_server_json, int chainno);
bool get_alive_server_json_with_chainno(Json::Value server_list_json,
                                  	Json::Value& result_server_json,
                                  	int chainno);
void send_req_to_extend_server();                          

#endif

