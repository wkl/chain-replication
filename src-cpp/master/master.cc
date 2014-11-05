#include "master.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<Master> master;
int check_alive_interval;
int crash_timeout;

void MasterTCPLoop::handle_msg(proto::Message& msg, proto::Address& from_addr) {
  rec_msg_seq++;
  LOG(INFO) << "Master received tcp message from " << from_addr.ip() << ":"
      << from_addr.port() << ", recv_req_seq = " << rec_msg_seq << endl
      << msg.ShortDebugString() << endl << endl;

  switch (msg.type()) {
    case proto::Message::HEARTBEAT:
      assert(msg.has_heartbeat());
      master->handle_heartbeat(msg.heartbeat());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

void MasterUDPLoop::handle_msg(proto::Message& msg, proto::Address& from_addr) {
  rec_msg_seq++;
  switch (msg.type()) {
    case proto::Message::HEARTBEAT:
      assert(msg.has_heartbeat());
      master->handle_heartbeat(msg.heartbeat());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

Node* BankServerChain::get_node(const proto::Address& addr) {
  for (auto &node : server_chain_)
    if (node == addr)
      return &node;

  LOG(INFO) << "node not found:" << addr.ShortDebugString() << endl;
  return nullptr;
}

void Master::handle_heartbeat(const proto::Heartbeat& hb) {
  BankServerChain& bsc = get_bank_server_chain_by_id(hb.bank_id());
  Node *node = bsc.get_node(hb.server_addr());
  if (node && !node->is_crashed())
    node->set_report_time();
}

bool Node::become_crashed() {
  assert(!crashed_);
  if (!reported_)
    return false;

  auto now = steady_clock::now();
  duration<double> time_span =
      duration_cast<duration<double>>(now - last_report_time_);

  if (time_span.count() > crash_timeout)
    crashed_ = true;

  return crashed_;
}

void check_alive() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(check_alive_interval));
    for (auto &bsc : master->bank_server_chain())
      for (auto &node : bsc.second.server_chain())
        if (!node.is_crashed() && node.become_crashed()) {
          // TODO notify
          LOG(INFO) << bsc.first << " " << node.addr().ShortDebugString()
                    << " become crashed!" << endl;
        }
  }
}

int read_config_master(string dir) {
  Json::Reader reader;
  Json::Value root;
  std::ifstream ifs;

  ifs.open(dir, std::ios::binary);
  if (!ifs.is_open()) {
    LOG(ERROR) << "Fail to open the config file: " << dir << endl << endl;
    return 1;
  }

  if (!reader.parse(ifs, root)) {
    ifs.close();
    LOG(ERROR) << "Fail to parse the config file: " << dir << endl << endl;
    return 1;
  }

  Json::Value config_json = root[JSON_CONFIG];
  // config info
  check_alive_interval = config_json[JSON_SERVER_REPORT_INTERVAL].asInt();  
  crash_timeout = config_json[JSON_SERVER_FAIL_TIMEOUT].asInt(); 
  Json::Value master_json = root[JSON_MASTER];
  proto::Address master_addr;  
  // master address
  master_addr.set_ip(master_json[JSON_IP].asString());
  master_addr.set_port(master_json[JSON_PORT].asInt());
  master->set_addr(master_addr);
  // bank list
  Json::Value bank_list_json = root[JSON_BANKS];
  for (unsigned int i = 0; i < bank_list_json.size(); i++) {
    Json::Value bank_json = bank_list_json[i];
    BankServerChain bsc;
    bsc.set_bank_id(bank_json[JSON_BANKID].asString());
    Json::Value server_list_json = bank_json[JSON_SERVERS];
    for (unsigned int j = 0; j < server_list_json.size(); j++) {
      Json::Value server_json = server_list_json[j];
      Node node(server_json[JSON_IP].asString(), server_json[JSON_PORT].asInt());
      bsc.append_node(node);  // add server (only running server in the initialization)
      if (server_json[JSON_CHAINNO].asInt() == 1)  // it is a header server
        bsc.set_head(node);
      if (server_json[JSON_CHAINNO].asInt() == server_list_json.size())  // it is a tail server
        bsc.set_tail(node);
    }
    master->add_bank(bsc.bank_id(), bsc);
  }
  // client list
  Json::Value client_list_json = root[JSON_CLIENTS];
  for (unsigned int i = 0; i < client_list_json.size(); i++) {
    Json::Value client_json = client_list_json[i];
    proto::Address client_addr; 
    client_addr.set_ip(client_json[JSON_IP].asString());
    client_addr.set_port(client_json[JSON_PORT].asInt());
    master->add_client(client_json[JSON_CLIENTID].asString(), client_addr);
  }

  LOG(INFO) << "Master initialization: " 
            << "master ip: " << master->addr().ip() << ", "
	    << "master port: " << master->addr().port() << ", "
	    << "server report interval: " << check_alive_interval << " sec, "
            << "server fail timeout: " << crash_timeout << " sec"
	    << endl << endl;

  return 0;
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

    if (!vm.count("config-file")) {
        LOG(ERROR) << "Please input the config-file path" << endl << endl;
        return 1;
    }	

    FLAGS_logtostderr = true;
    if (vm.count("log-dir")) {
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

    // read configuration file
    if (read_config_master(vm["config-file"].as<string>()) != 0 )
      return 1;

    /*
    MasterTCPLoop tcp_loop(master->addr().port());
    MasterUDPLoop udp_loop(master->addr().port());
    std::thread tcp_thread(tcp_loop);
    std::thread udp_thread(udp_loop);
    std::thread check_alive_thread(tcp_loop);
    tcp_thread.join();
    udp_thread.join();
    check_alive_thread.join();
    */

    /*
    // begin test data
    BankServerChain bsc1;
    BankServerChain bsc2;
    BankServerChain bsc3;
    Node node1("127.0.0.1", 50001);
    bsc1.append_node(node1);
    bsc1.set_head(node1);
    bsc1.set_tail(node1);
    Node node2("127.0.0.1", 50002);
    Node node3("127.0.0.1", 50003);
    bsc2.append_node(node2).append_node(node3);
    bsc2.set_head(node2);
    bsc2.set_tail(node3);
    Node node4("127.0.0.1", 50004);
    Node node5("127.0.0.1", 50005);
    Node node6("127.0.0.1", 50006);
    bsc3.append_node(node4).append_node(node5).append_node(node6);
    bsc3.set_head(node4);
    bsc3.set_tail(node6);
    master->add_bank("bank1", bsc1);
    master->add_bank("bank2", bsc2);
    master->add_bank("bank3", bsc3);

    MasterTCPLoop tcp_loop(50000);
    std::thread tcp_thread(tcp_loop);
    MasterUDPLoop udp_loop(50000);
    std::thread udp_thread(udp_loop);

    //check_alive_interval = 5;
    //crash_timeout = 5;
    std::thread check_alive_thread(check_alive);

    tcp_thread.join();
    udp_thread.join();
    check_alive_thread.join();
    // end test data
    */

    /* TODO read data from config file
    if (vm.count("config-file")) {
      MasterTCPLoop tcp_loop(master->addr().port());
      MasterUDPLoop udp_loop(master->addr().port());
      std::thread tcp_thread(tcp_loop);
      std::thread udp_thread(udp_loop);
      std::thread check_alive_thread(tcp_loop);
      tcp_thread.join();
      udp_thread.join();
      check_alive_thread.join();
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
