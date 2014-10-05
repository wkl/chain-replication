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
  ChainServer(std::string bank_id) : bank_id(bank_id) {
    std::cout << bank_id << std::endl;
  }

  void forward_request(const Request& req) {
    std::cout << "forwarding request..." << std::endl;
    send_msg_tcp(Node("127.0.0.1", 50002), Message::REQUEST, req);
  }

 private:
  std::string bank_id;
};

class ChainServerUDPLoop : public UDPLoop {
  using UDPLoop::UDPLoop;  // inherit constructor
  void handle_msg(Message &msg);
};

class ChainServerTCPLoop : public TCPLoop {
  using TCPLoop::TCPLoop;
  void handle_msg(Message &msg);
};

#endif
