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

#include "common.h"
#include "message.h"
#include "server.h"

using std::string;
using std::unordered_map;

class Client {
 public:
  // constructor
  Client(string ip, int port) : ip_(ip), port_(port) {};

  // thread entry
  void operator()() { run(); };
  void run();

  void receive_reply(const proto::Reply& reply);
  void handle_msg(proto::Message& msg);

 private:
  string ip_;
  int port_;
  unordered_map<string,ChainServer> bank_head_list_;
  unordered_map<string,ChainServer> bank_tail_list_;
};

#endif

