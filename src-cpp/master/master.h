/**
 * @file  master.h
 * @brief
 */

#ifndef MASTER_H
#define MASTER_H

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <list>
#include <chrono>

#include "common.h"
#include "message.h"
#include "bank.h"

using std::string;
using std::unordered_map;
using std::list;
using std::cout;
using std::endl;
using std::cerr;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;

// server information on Master
class Node {
 public:
  Node(string ip, int port) : reported_(false), crashed_(false){
    addr_.set_ip(ip);
    addr_.set_port(port);
  };

  proto::Address& addr() { return addr_; };
  bool operator==(const proto::Address& addr) {
    return addr_.ip() == addr.ip() && addr_.port() == addr.port();
  }
  void set_report_time() {
    last_report_time_ = steady_clock::now();
    reported_ = true;
  }
  bool is_crashed() { return crashed_; };
  bool has_reported() { return reported_; };
  bool become_crashed();

 private:
  proto::Address addr_;
  steady_clock::time_point last_report_time_;
  bool reported_;
  bool crashed_;
};

class BankServerChain {
 public:
  BankServerChain(){};
  BankServerChain& append_node(Node node) {
    server_chain_.push_back(node);
    return *this;
  }
  void set_head(Node node) { head_.CopyFrom(node.addr()); }
  void set_head(string ip, int port) {
    head_.set_ip(ip);
    head_.set_port(port);
  };
  void set_tail(Node node) { tail_.CopyFrom(node.addr()); }
  void set_tail(string ip, int port) {
    tail_.set_ip(ip);
    tail_.set_port(port);
  };
  proto::Address& head() { return head_; };
  proto::Address& tail() { return tail_; };

  Node* get_node(const proto::Address& addr);
  list<Node>& server_chain() { return server_chain_; };

 private:
  proto::Address head_;
  proto::Address tail_;
  list<Node> server_chain_;
};

class Master {
 public:
  Master(){};

  void handle_heartbeat(const proto::Heartbeat& hb);
  void add_bank(string bank_id, BankServerChain& bsc) {
    auto it = bank_server_chain_.insert(std::make_pair(bank_id, bsc));
    assert(it.second);
  }
  BankServerChain& get_bank_server_chain_by_id(string bank_id) {
    auto it = bank_server_chain_.find(bank_id);
    assert(it != bank_server_chain_.end());
    return it->second;
  }

  // getter/setter
  void set_addr(proto::Address addr) { addr_ = addr; };
  proto::Address& addr() { return addr_; };
  unordered_map<string, BankServerChain>& bank_server_chain() {
    return bank_server_chain_;
  };

 private:
  proto::Address addr_;
  // <bankid, chain>
  unordered_map<string, BankServerChain> bank_server_chain_;
};

class MasterTCPLoop : public TCPLoop {
  using TCPLoop::TCPLoop;
  void handle_msg(proto::Message& msg, proto::Address& from_addr);
};

class MasterUDPLoop : public UDPLoop {
  using UDPLoop::UDPLoop;
  void handle_msg(proto::Message& msg, proto::Address& from_addr);
};

#endif
