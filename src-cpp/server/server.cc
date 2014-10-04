#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include "message.pb.h"

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::address;
namespace po = boost::program_options;

const int max_length = 1024;

class ChainServer;
std::unique_ptr<ChainServer> cs;

class ChainServer {
 public:
  ChainServer(std::string bank_id) : bank_id(bank_id) {
    std::cout << bank_id << std::endl;
  }

  void forward_request(char* data, int length) {
    boost::asio::io_service io_service;

    tcp::socket s(io_service);
    tcp::endpoint endpoint(address::from_string("127.0.0.1"), 50002);
    s.connect(endpoint);

    boost::asio::write(s, boost::asio::buffer(data, length));
  }

 private:
  std::string bank_id;
};

class UDPLoop {
 public:
  UDPLoop(int port) : port(port) {}

  void operator()() {
    boost::asio::io_service io_service;
    udp::socket sock(io_service, udp::endpoint(udp::v4(), port));
    for (;;) {
      char data[max_length];
      udp::endpoint sender_endpoint;
      size_t length = sock.receive_from(boost::asio::buffer(data, max_length),
                                        sender_endpoint);
      std::cout << "UDP server received:" << std::string(data, length)
                << std::endl;
      sock.send_to(boost::asio::buffer(data, length), sender_endpoint);
      cs->forward_request(data, length);
    }
  }

 private:
  int port;
};

class TCPLoop {
 public:
  TCPLoop(int port) : port(port) {}

  void operator()() {
    boost::asio::io_service io_service;
    tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
    for (;;) {
      tcp::socket sock(io_service);
      a.accept(sock);
      std::thread(&TCPLoop::session, this, std::move(sock)).detach();
    }
  }

  void session(tcp::socket sock) {
    try {
      for (;;) {
        char data[max_length];

        boost::system::error_code error;
        size_t length = sock.read_some(boost::asio::buffer(data), error);
        if (error == boost::asio::error::eof)
          break;  // Connection closed cleanly by peer.
        else if (error)
          throw boost::system::system_error(error);  // Some other error.

        std::cout << "TCP server received:" << std::string(data, length)
                  << std::endl;
        boost::asio::write(sock, boost::asio::buffer(data, length));
      }
    } catch (std::exception& e) {
      std::cerr << "Exception in thread: " << e.what() << "\n";
    }
  }

 private:
  int port;
};

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<std::string>(), "specify config file path")(
        "second,s", "I'm second chain server");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    cs = std::unique_ptr<ChainServer>(new ChainServer("boa"));

    int server_port = 50001;
    if (vm.count("second")) server_port = 50002;
    UDPLoop udp_loop(server_port);
    TCPLoop tcp_loop(server_port);
    std::thread udp_thread(udp_loop);
    std::thread tcp_thread(tcp_loop);
    udp_thread.join();
    tcp_thread.join();

    /*
    TODO read config file
    if (vm.count("config-file")) {
      ifstream ifs(vm["config-file"].as<string>());
      if (!ifs) {
        cerr << "open '" << vm["config-file"].as<string>() << "' failed"
             << endl;
        return 1;
      }
      string line;
      getline(ifs, line);
      cout << line;
    } else {
      cerr << "config-file was not set." << endl;
      return 1;
    }
    */
  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

