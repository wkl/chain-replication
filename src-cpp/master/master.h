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

// server information on Master
class Node {
  Node() : reported_(false), crashed_(false){};

 private:
  proto::Address addr_;
  steady_clock::time_point last_report_time_;
  bool reported_;
  bool crashed_;
};

class BankServerChain {
 public:
  BankServerChain(){};

 private:
  proto::Address head_;
  proto::Address tail_;
  list<Node> server_chain_;
};

class Master {
 public:
  Master(){};

  void handle_heartbeat(const proto::Heartbeat& hb);

  // getter/setter
  void set_addr(proto::Address addr) { addr_ = addr; };
  proto::Address& addr() { return addr_; };

 private:
  proto::Address addr_;
  // <bankid, chain>
  unordered_map<string, BankServerChain> bank_server_chain_;
};

class MasterTCPLoop : public TCPLoop {
  using TCPLoop::TCPLoop;
  void handle_msg(proto::Message& msg, proto::Address& from_addr);
};

#endif
