#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::address;
namespace po = boost::program_options;

const int max_length = 1024;

class Client {
 public:
  void operator()() {
    try {
      send_udp_request();
    } catch (std::exception& e) {
      std::cerr << "error: " << e.what() << std::endl;
    }
  }

  void send_tcp_request() {
    boost::asio::io_service io_service;

    tcp::socket s(io_service);
    tcp::endpoint endpoint(address::from_string("127.0.0.1"), 50001);
    s.connect(endpoint);

    std::cout << "Enter message: ";
    char request[max_length];
    std::cin.getline(request, max_length);
    size_t request_length = std::strlen(request);
    boost::asio::write(s, boost::asio::buffer(request, request_length));

    char reply[max_length];
    size_t reply_length =
        boost::asio::read(s, boost::asio::buffer(reply, request_length));
    std::cout << "Reply is: ";
    std::cout.write(reply, reply_length);
    std::cout << "\n";
  }

  void send_udp_request() {
    boost::asio::io_service io_service;
    udp::socket s(io_service, udp::endpoint(udp::v4(), 0));
    udp::endpoint endpoint(address::from_string("127.0.0.1"), 50001);

    std::cout << "Enter UDP message: ";
    char request[max_length];
    std::cin.getline(request, max_length);
    size_t request_length = std::strlen(request);
    s.send_to(boost::asio::buffer(request, request_length), endpoint);

    char reply[max_length];
    udp::endpoint server_endpoint;
    size_t reply_length =
        s.receive_from(boost::asio::buffer(reply, max_length), server_endpoint);
    std::cout << "Reply is: ";
    std::cout.write(reply, reply_length);
    std::cout << "\n";
  }
};

int main(int argc, char* argv[]) {
  Client c;
  std::thread t(c);
  t.join();
  return 0;
}

