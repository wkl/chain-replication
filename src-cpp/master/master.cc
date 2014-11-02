#include "master.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<Master> master;

void MasterTCPLoop::handle_msg(proto::Message& msg, proto::Address& from_addr) {
  rec_msg_seq++;
  switch (msg.type()) {
    case proto::Message::HEARTBEAT:
      assert(msg.has_heartbeat());
      LOG(INFO) << "Master received tcp message from " << from_addr.ip() << ":"
                << from_addr.port() << ", rec_req_seq = " << rec_msg_seq << endl
                << msg.ShortDebugString() << endl << endl;
      master->handle_heartbeat(msg.heartbeat());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

void Master::handle_heartbeat(const proto::Heartbeat& hb) {
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<string>(), "specify the config-file path")(
        "log-dir,l", po::value<string>(), "specify logging dir");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << endl;
      return 0;
    }

    FLAGS_logtostderr = true;
    if (vm.count("log-dir") && vm.count("config-file")) {
      FLAGS_log_dir = vm["log-dir"].as<string>();
      FLAGS_logtostderr = false;
      FLAGS_logbuflevel = -1;
      string log_name = "master";
      google::InitGoogleLogging(log_name.c_str());
    } else {
      FLAGS_logbuflevel = -1;
      google::InitGoogleLogging(argv[0]);
    }

    LOG(INFO) << "Processing configuration file" << endl << endl;

    master = std::unique_ptr<Master>(new Master());

    MasterTCPLoop tcp_loop(50000);
    std::thread tcp_thread(tcp_loop);
    tcp_thread.join();

    /* TODO read config file
    if (vm.count("config-file")) {
      MasterTCPLoop tcp_loop(master->addr().port());
      std::thread tcp_thread(tcp_loop);
      tcp_thread.join();
    } else {
      LOG(ERROR) << "Please input the config-file path" << endl << endl;
      return 1;
    }
    */

  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}
