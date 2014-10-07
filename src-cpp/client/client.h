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

#include "common.h"
#include "message.h"

class Client {
 public:
  Client(std::string ip, int port) : ip_(ip), port_(port) {}

  // thread entry
  void operator()() { run(); };
  void run();

  void receive_reply(const proto::Reply& reply);
  void handle_msg(proto::Message& msg);

 private:
  std::string ip_;
  int port_;
};

#endif

