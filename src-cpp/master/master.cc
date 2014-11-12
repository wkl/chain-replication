#include "master.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

std::unique_ptr<Master> master;
int check_alive_interval;
int crash_timeout;

void notify_internal_crash_to_preserver(const proto::Reqseq& req_seq);

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
    case proto::Message::REQ_SEQ:
      assert(msg.has_reqseq());
      notify_internal_crash_to_preserver(msg.reqseq());
      break;
    case proto::Message::JOIN:
      assert(msg.has_join());
      master->handle_join(msg.join());
      break;
    case proto::Message::EXTEND_FINISH:
      assert(msg.has_extend_finish());
      master->handle_extend_finish(msg.extend_finish());
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

  //LOG(INFO) << "node not found:" << addr.ShortDebugString() << endl;
  return nullptr;
}

// master receive heartbeat from server
void Master::handle_heartbeat(const proto::Heartbeat& hb) {
  BankServerChain& bsc = get_bank_server_chain_by_id(hb.bank_id());
  Node *node = bsc.get_node(hb.server_addr());
  if (node && !node->is_crashed())
    node->set_report_time();
  /*
  LOG(INFO) << "Receive heartbeat from server " 
            << hb.server_addr().ip() << ":" << hb.server_addr().port() 
            << " of bank " << hb.bank_id() 
            << endl << endl;
  */
}

// master receive join request from server
void Master::handle_join(const proto::Join& join) {
  BankServerChain& bsc = get_bank_server_chain_by_id(join.bank_id());
  Node node(join.server_addr().ip(), join.server_addr().port());
  bsc.append_node(node); 
  LOG(INFO) << "Receive join request from server " 
            << join.server_addr().ip() << ":" << join.server_addr().port() 
            << " of bank " << join.bank_id() 
            << endl << endl;
  send_msg_tcp(bsc.pre_server_addr(node), proto::Message::EXTEND_SERVER,
               join.server_addr()); 
  bsc.set_extending(node);             
}

// master receive msg of extending finish from extended server
void Master::handle_extend_finish(const proto::ExtendFinish& extend_finish) {
  BankServerChain& bsc = get_bank_server_chain_by_id(extend_finish.bank_id());
  Node node(extend_finish.server_addr().ip(), extend_finish.server_addr().port());
  bsc.set_tail(node);
  Node emptyNode("", 0);
  bsc.set_extending(emptyNode);
  LOG(INFO) << "Receive msg from extended server "
            << extend_finish.server_addr().ip() << ":" << extend_finish.server_addr().port()
            <<" that it begins to act as tail server"
            << endl << endl;
  proto::Notify notify;
  auto *new_tail_addr = new proto::Address;
  new_tail_addr->CopyFrom(extend_finish.server_addr());
  notify.set_bank_id(bsc.bank_id());
  notify.set_allocated_server_addr(new_tail_addr);
  for(const auto& it : master->client_list()) {
      send_msg_udp(master->addr(), it.second, proto::Message::NEW_TAIL, notify);
  }   
  LOG(INFO) << "Notify all the clients of the new tail server" 
            << " of bank " << bsc.bank_id()
            << endl << endl;       
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

void notify_crash(BankServerChain& bsc, Node& node) {
  if (node == bsc.head() && node != bsc.tail()) {         // head crashed
    proto::Message empty_msg;
    auto *new_head_addr = new proto::Address;
    new_head_addr->CopyFrom(bsc.succ_server_addr(node));
    send_msg_tcp(*new_head_addr, proto::Message::TO_BE_HEAD,
                 empty_msg);
    bsc.set_head(bsc.succ_server_addr(node).ip(), bsc.succ_server_addr(node).port());             
    bsc.remove_node(node);
    LOG(INFO) << "Notify server " 
              << new_head_addr->ip() << ":" << new_head_addr->port() 
              << " of bank " << bsc.bank_id() << " to be new head server" 
              << endl << endl;
    // notify all clients and tail servers of the new head server
    proto::Notify notify;
    notify.set_bank_id(bsc.bank_id());
    notify.set_allocated_server_addr(new_head_addr);
    for(const auto& it : master->client_list()) {
      send_msg_udp(master->addr(), it.second, proto::Message::NEW_HEAD, notify);
    }   
    for(const auto& it : master->bank_server_chain()) {
      BankServerChain tmp_bsc = it.second;
      send_msg_tcp(tmp_bsc.tail(), proto::Message::NEW_HEAD, notify);
    }
    LOG(INFO) << "Notify all the clients and tail servers of the new head server" 
              << " of bank " << bsc.bank_id()
              << endl << endl;
  } else if (node != bsc.head() && node == bsc.tail()) {  // tail crashed
    proto::Message empty_msg;
    auto *new_tail_addr = new proto::Address;
    new_tail_addr->CopyFrom(bsc.pre_server_addr(node));
    send_msg_tcp(*new_tail_addr, proto::Message::TO_BE_TAIL,
                 empty_msg);
    bsc.set_tail(bsc.pre_server_addr(node).ip(), bsc.pre_server_addr(node).port());             
    bsc.remove_node(node);
    LOG(INFO) << "Notify server " 
              << new_tail_addr->ip() << ":" << new_tail_addr->port() 
              << " of bank " << bsc.bank_id() << " to be new tail server" 
              << endl << endl;  
    // notify all clients of the new tail server
    proto::Notify notify;
    notify.set_bank_id(bsc.bank_id());
    notify.set_allocated_server_addr(new_tail_addr);
    for(const auto& it : master->client_list()) {
      send_msg_udp(master->addr(), it.second, proto::Message::NEW_TAIL, notify);
    }   
    LOG(INFO) << "Notify all the clients of the new tail server" 
              << " of bank " << bsc.bank_id()
              << endl << endl;  
    // if there's a extending server
    Node empty_node("", 0);
    if (!(empty_node == bsc.extending())) {
      LOG(INFO) << "Notify new tail server that there is a server which wants to join"
                << endl << endl;
      send_msg_tcp(bsc.tail(), proto::Message::EXTEND_SERVER,
                   bsc.extending()); 
    }                          
  } else if (node != bsc.head() && node != bsc.tail() && node!=bsc.extending()) {  // internal crashed
    send_msg_tcp(bsc.succ_server_addr(node), proto::Message::NEW_PRE_SERVER,
                 bsc.pre_server_addr(node));
    bsc.remove_node(node);
    LOG(INFO) << "Notify S+ server of bank " << bsc.bank_id() 
              << " that S server has crashed" 
              << endl <<endl;
  } else {  // extending server crashed
    assert(node==bsc.extending());
    proto::Message empty_msg;
    send_msg_tcp(bsc.pre_server_addr(node), proto::Message::EXTEND_FAIL,
                 empty_msg); 
    bsc.remove_node(node);
    Node emptyNode("", 0);
    bsc.set_extending(emptyNode);  
    LOG(INFO) << "Notify current tail server of bank " << bsc.bank_id()
              << " that new extending server has crashed"
              << endl << endl;
  }
}

void notify_internal_crash_to_preserver(const proto::Reqseq& req_seq) {
  send_msg_tcp(req_seq.pre_addr(), proto::Message::NEW_SUCC_SERVER, req_seq);
  LOG(INFO) << "Notify S- server of bank " << req_seq.bank_id()
            << " that S server has crashed with req_seq="
            << req_seq.bank_update_seq()
            << endl << endl;
}

void check_alive() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(check_alive_interval));
    for (auto& bsc : master->bank_server_chain())
      for (auto& node : bsc.second.server_chain())
        if (!node.is_crashed() && node.become_crashed()) {
          LOG(INFO) << bsc.first << " " << node.addr().ShortDebugString()
                    << " become crashed!" << endl << endl;
          notify_crash(bsc.second, node);
          break;
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
    Node empty_node("", 0);
    bsc.set_extending(empty_node);
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
            << "master port: " << master->addr().port() << endl << endl;

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
    
    MasterTCPLoop tcp_loop(master->addr().port());
    std::thread tcp_thread(tcp_loop);
    MasterUDPLoop udp_loop(master->addr().port());
    std::thread udp_thread(udp_loop);
    std::thread check_alive_thread(check_alive);
    tcp_thread.join();
    udp_thread.join();
    check_alive_thread.join();
    
    
    // begin test data
    /*
    BankServerChain bsc1;
    Node node1("127.0.0.1", 50001);
    Node node2("127.0.0.1", 50002);
    bsc1.append_node(node1).append_node(node2);
    bsc1.set_head(node1);
    bsc1.set_tail(node2);
    master->add_bank("bank1", bsc1);*/
    /*
    MasterTCPLoop tcp_loop(50000);
    std::thread tcp_thread(tcp_loop);
    MasterUDPLoop udp_loop(50000);
    std::thread udp_thread(udp_loop);

    check_alive_interval = 5;
    crash_timeout = 5;
    std::thread check_alive_thread(check_alive);

    tcp_thread.join();
    udp_thread.join();
    check_alive_thread.join();
    // end test data
    */
    
  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}
