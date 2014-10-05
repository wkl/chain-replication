#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

class ChainServer;
std::unique_ptr<ChainServer> cs;

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

void ChainServerUDPLoop::handle_msg(Message& msg) {
  switch (msg.type()) {
    case Message::REQUEST:
      assert(msg.has_request());
      cs->forward_request(msg.request());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServerTCPLoop::handle_msg(Message& msg) {
  switch (msg.type()) {
    case Message::REQUEST:
      assert(msg.has_request());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

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
    ChainServerUDPLoop udp_loop(server_port);
    ChainServerTCPLoop tcp_loop(server_port);
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

