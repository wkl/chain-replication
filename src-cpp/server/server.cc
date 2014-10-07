#include "server.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<ChainServer> cs;

// variables for debug
bool is_head;
bool is_tail;

void ChainServerUDPLoop::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServerTCPLoop::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REQUEST:
      assert(msg.has_request());
      cs->receive_request(msg.mutable_request());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void ChainServer::forward_request(proto::Request* req) {
  req->set_bank_update_seq(++bank_update_seq_);
  std::cout << "forwarding request..." << std::endl;
  send_msg_tcp(Node("127.0.0.1", 50002), proto::Message::REQUEST, *req);
}

void ChainServer::reply(const proto::Request& req) {
  proto::Reply reply;
  reply.set_outcome(proto::Reply::PROCESSED);
  reply.set_req_id(req.req_id());
  Node client(req.client_addr().ip(), req.client_addr().port());
  send_msg_udp(client, proto::Message::REPLY, reply);
}

void ChainServer::receive_request(proto::Request* req) {
  if (is_head) cs->forward_request(req);
  if (is_tail) cs->reply(*req);
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<std::string>(), "specify config file path")(
        "log-dir,l", po::value<std::string>(), "specify logging dir")(
        "second,s", "I'm second chain server");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    FLAGS_logtostderr = true;
    if (vm.count("log-dir")) {
      FLAGS_log_dir = vm["log-dir"].as<std::string>();
      FLAGS_logtostderr = false;
    }
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "Processing configuration file";

    cs = std::unique_ptr<ChainServer>(new ChainServer("boa"));

    int server_port;
    if (vm.count("second")) {
      is_tail = true;
      is_head = false;
      server_port = 50002;
    } else {
      is_tail = false;
      is_head = true;
      server_port = 50001;
    }

    if (vm.count("config-file")) {
      std::ifstream ifs(vm["config-file"].as<std::string>(), std::ios::binary);
      if (!ifs) {
        std::cerr << "open '" << vm["config-file"].as<std::string>() << "' failed"
                  << std::endl;
        return 1;
      }

      Json::Reader reader;
      Json::Value root;
      assert(reader.parse(ifs, root));
    }
    /*
    else {
      cerr << "config-file was not set." << endl;
      return 1;
    }
    */

    ChainServerUDPLoop udp_loop(server_port);
    ChainServerTCPLoop tcp_loop(server_port);
    std::thread udp_thread(udp_loop);
    std::thread tcp_thread(tcp_loop);
    udp_thread.join();
    tcp_thread.join();

  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

