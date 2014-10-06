#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include "common.h"
#include "message.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

class Client {
 public:
  Client(std::string ip, int port) : ip_(ip), port_(port) {}

  void operator()() {
    try {
      proto::Request req;
      req.set_type(proto::Request::QUERY);
      req.set_req_id("boa.1.1");
      proto::Address* addr = new proto::Address;
      addr->set_ip(ip_);
      addr->set_port(port_);
      req.set_allocated_client_addr(addr);

      Node head("127.0.0.1", 50001);
      send_msg_udp(head, proto::Message::REQUEST, req);
    } catch (std::exception& e) {
      std::cerr << "error: " << e.what() << std::endl;
    }
  }

 private:
  std::string ip_;
  int port_;
};

int main(int argc, char* argv[]) {
  Client c("127.0.0.1", 60001);
  std::thread t(c);
  t.join();
  return 0;
}

