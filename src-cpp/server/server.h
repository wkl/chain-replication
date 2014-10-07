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

#include "common.h"
#include "message.h"

class ChainServer {
 public:
  ChainServer(std::string bank_id) : bank_id_(bank_id) {
    std::cout << bank_id << std::endl;
    bank_update_seq_ = 0;
  }

  void receive_request(proto::Request *req);

  // test function, to be removed
  void forward_request(proto::Request *req);
  // test function, to be removed
  void reply(const proto::Request &req);

 private:
  std::string bank_id_;
  int bank_update_seq_;
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

