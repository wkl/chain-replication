/**
 * @file  client.h
 * @brief
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <ctime>

#include "common.h"
#include "message.h"
#include "server.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::cout;
using std::endl;
using std::cerr;

class Client {
 public:
  // constructor
  Client(){};
  Client(string ip, int port) : ip_(ip), port_(port){};

  // thread entry
  void operator()() { run(); };
  void run();

  void handle_msg(proto::Message& msg);
  void handle_new_head(const proto::Notify& notify);
  void handle_new_tail(const proto::Notify& notify);

  // getter/setter
  void set_ip(string ip) { ip_ = ip; };
  string ip() { return ip_; };
  void set_port(int port) { port_ = port; };
  int port() { return port_; };
  void set_clientid(string clientid) { clientid_ = clientid; };
  string clientid() { return clientid_; };
  void set_wait_timeout(int wait_timeout) { wait_timeout_ = wait_timeout; };
  int wait_timeout() { return wait_timeout_; };
  void set_resend_num(int resend_num) { resend_num_ = resend_num; };
  int resend_num() { return resend_num_; };
  void set_resend_newhead(bool resend_newhead) { resend_newhead_ = resend_newhead; };
  bool resend_newhead() { return resend_newhead_; };
  void set_drop_interval(int drop_interval) { drop_interval_ = drop_interval; };
  int drop_interval() { return drop_interval_; };
  void set_recv_count(int recv_count) { recv_count_ = recv_count; };
  int recv_count() { return recv_count_; };

  void set_request_vector(vector<proto::Request> request_vector) {
    request_vector_ = request_vector;
  };

  vector<proto::Request>& request_vector() { return request_vector_; };

  void set_bank_head_list(
      unordered_map<string, proto::Address> bank_head_list) {
    bank_head_list_ = bank_head_list;
  };

  unordered_map<string, proto::Address>& bank_head_list() {
    return bank_head_list_;
  };

  void set_bank_tail_list(
      unordered_map<string, proto::Address> bank_tail_list) {
    bank_tail_list_ = bank_tail_list;
  };

  unordered_map<string, proto::Address>& bank_tail_list() {
    return bank_tail_list_;
  };

  // return a copy of address
  proto::Address get_bank_head(const string& bankid) {
      auto it_head = bank_head_list_.find(bankid);
      assert(it_head != bank_head_list_.end());
      return it_head->second;
  }

  proto::Address get_bank_tail(const string& bankid) {
      auto it_tail = bank_tail_list_.find(bankid);
      assert(it_tail != bank_tail_list_.end());
      return it_tail->second;
  }
  
  bool drop_reply() {
    if (drop_interval_ == 0)
      return false;
    else {
      if (recv_count_ % drop_interval_ == 0)
        return true;
      else 
        return false;
    }
  }

 private:
  string ip_;
  int port_;
  string clientid_;
  int wait_timeout_;
  int resend_num_;
  bool resend_newhead_;
  int drop_interval_; // in seq
  int recv_count_;  
  unordered_map<string, proto::Address> bank_head_list_;  // <bankid, headaddr>
  unordered_map<string, proto::Address> bank_tail_list_;  // <bankid, tailaddr>
  vector<proto::Request> request_vector_;
};

// global
int read_config_client(string dir, vector<Client>& client_vector);

#endif

