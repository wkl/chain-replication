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
  void operator()() {
    try {
      Request req;
      req.set_type(Request::QUERY);
      req.set_req_id("boa.1.1");
      Node head("127.0.0.1", 50001);
      send_msg_udp(head, Message::REQUEST, req);
    } catch (std::exception& e) {
      std::cerr << "error: " << e.what() << std::endl;
    }
  }
};

int main(int argc, char* argv[]) {
  Client c;
  std::thread t(c);
  t.join();
  return 0;
}

