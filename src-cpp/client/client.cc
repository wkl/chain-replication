#include  <setjmp.h>
#include  "client.h"
#include  "udp_client.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

void Client::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::NEW_HEAD:
      assert(msg.has_notify());
      handle_new_head(msg.notify());
      break;
    case proto::Message::NEW_TAIL:
      assert(msg.has_notify());
      handle_new_tail(msg.notify());
      break;      
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

// receive notification of new head server of a specific bank
void Client::handle_new_head(const proto::Notify& notify) {
  string bank_id = notify.bank_id();
  proto::Address server_addr = notify.server_addr();
  auto it = bank_head_list_.find(bank_id);
  assert(it != bank_head_list_.end());
  it->second = server_addr;
  LOG(INFO) << clientid_ << ": Bank " << bank_id 
            << "'s head server is changed to "
            << server_addr.ip() << ":" << server_addr.port()
            << endl << endl;
}

// receive notification of new tail server of a specific bank
void Client::handle_new_tail(const proto::Notify& notify) {
  string bank_id = notify.bank_id();
  proto::Address server_addr = notify.server_addr();
  auto it = bank_tail_list_.find(bank_id);
  assert(it != bank_tail_list_.end());
  it->second = server_addr;
  LOG(INFO) << clientid_ << ": Bank " << bank_id 
            << "'s tail server is changed to "
            << server_addr.ip() << ":" << server_addr.port()
            << endl << endl;
}

void Client::run() {
  char data[UDP_MAX_LENGTH];
  udp::endpoint sender_endpoint;
  udp::endpoint listen_endpoint(address::from_string(ip_), port_);
  UDPReceiver receiver(listen_endpoint);

  try {
    for (auto req : request_vector_) {
      // have a sleep
      std::this_thread::sleep_for(std::chrono::seconds(1));
 
      // client address
      proto::Address* local_addr = new proto::Address;
      local_addr->set_ip(ip_);
      local_addr->set_port(port_);
      req.set_allocated_client_addr(local_addr);

      // check request type and send request
      proto::Address target;
      if (req.type() == proto::Request::QUERY)
        target = get_bank_tail(req.bank_id());
      else
        target = get_bank_head(req.bank_id()); 
        
      int retry = 0;
      bool accept_flag = false; // used to judge whether the reply is corresponding to current request 
      while(retry <= resend_num_ && !accept_flag) {
        send_msg_udp(*local_addr, target, proto::Message::REQUEST, req);
        LOG(INFO) << clientid_ << ": sent udp message to " 
                  << target.ip() << ":" << target.port() << endl
                  << req.ShortDebugString() << endl << endl;
        
        while (!accept_flag) {
          // receive
          boost::system::error_code ec;
          size_t length = receiver.receive(
              boost::asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint,
              boost::posix_time::seconds(wait_timeout_), ec);
          if (ec) { // timeout
            retry++;
            if (retry > resend_num_) {  // reaches maximum resend times, abort
              LOG(INFO) << clientid_ << ": retry " << resend_num_ 
                        << " times with no reply, abort request (" 
                        << req.req_id() << ")" << endl << endl;
            }
            else {  // resend
              if (resend_newhead_) {  // try new head / tail to resend
                if (req.type() == proto::Request::QUERY)
                  target = get_bank_tail(req.bank_id());
                else
                  target = get_bank_head(req.bank_id());
              }
              LOG(INFO) << clientid_ << ": timeout, retrying request (" 
                        << req.req_id() << ", " 
                        << retry << " of "
                        << resend_num_ << ")" 
                        << endl << endl;
            }          
            break;
          }
          proto::Message msg;
          assert(decode_msg(msg, data, length));
          if (msg.type() == proto::Message::REPLY) {  // msg is a reply
            recv_count_++;
            bool drop_this_reply = drop_reply();
            if (drop_this_reply) {   // drop packet on purpose
              LOG(INFO) << clientid_ << ": reply packet dropped, req_id=" 
                        << msg.reply().req_id() 
                        << endl << endl;
            }
            else {  // receive packet
              if (msg.reply().req_id() == req.req_id()) // accept packet
                accept_flag = true;
              LOG(INFO) << clientid_ << ": received udp message from "
                        << sender_endpoint << endl 
                        << msg.ShortDebugString() << endl << endl;
            }
          }
          else {  // msg is other notification
            LOG(INFO) << clientid_ << ": received udp message from "
                      << sender_endpoint << endl 
                      << msg.ShortDebugString() << endl << endl;
            handle_msg(msg);
          }
        } // end of while (!accept_flag)
      }  // end of while(retry <= resend_num_ && !accept_flag)
    }
  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
  }
}

// read configuration file for clients
int read_config_client(string dir, vector<Client>& client_vector) {
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
  
  srand((unsigned)time(0));
  Json::Value client_list_json;
  client_list_json = root[JSON_CLIENTS];
  for (unsigned int i = 0; i < client_list_json.size(); i++) {
    Json::Value client_json = client_list_json[i];
    // client basic info
    Client client;
    client.set_clientid(client_json[JSON_CLIENTID].asString());
    client.set_ip(client_json[JSON_IP].asString());
    client.set_port(client_json[JSON_PORT].asInt());
    client.set_wait_timeout(client_json[JSON_WAIT_TIMEOUT].asInt());
    client.set_resend_num(client_json[JSON_RESEND_NUM].asInt());
    client.set_resend_newhead(client_json[JSON_RESEND_NEWHEAD].asBool());
    LOG(INFO) << "New client initialization: "
              << " clientid=" << client.clientid() << ", ip=" << client.ip()
              << ", port=" << client.port()  << endl << endl;
    // bank info
    Json::Value bank_list_json = root[JSON_BANKS];
    for (unsigned int j = 0; j < bank_list_json.size(); j++) {
      Json::Value bank_json = bank_list_json[j];
      Json::Value server_list_json = bank_json[JSON_SERVERS];
      proto::Address head_addr;  // head server address
      head_addr.set_ip(server_list_json[0][JSON_IP].asString());
      head_addr.set_port(server_list_json[0][JSON_PORT].asInt());
      proto::Address tail_addr;  // tail server address
      tail_addr.set_ip(
           server_list_json[server_list_json.size() - 1][JSON_IP].asString());
      tail_addr.set_port(
           server_list_json[server_list_json.size() - 1][JSON_PORT].asInt());
      auto it_head = client.bank_head_list().insert(
           std::make_pair(bank_json[JSON_BANKID].asString(), head_addr));
      assert(it_head.second);
      auto it_tail = client.bank_tail_list().insert(
           std::make_pair(bank_json[JSON_BANKID].asString(), tail_addr));
      assert(it_tail.second);
    }
    // request info
    vector<proto::Request> request_vector;
    if (!client_json.isMember(JSON_REQSEED)) {  // read requests from file
        Json::Value request_json_list = client_json[JSON_REQUESTS];
      for (unsigned int j = 0; j < request_json_list.size(); j++) {
        Json::Value request_json = request_json_list[j];
        proto::Request req;
        req.set_bank_id(request_json[JSON_BANKID].asString());
        req.set_account_id(request_json[JSON_ACCOUNTID].asString());
        string req_id = req.bank_id() + "." + client.clientid() + "." +
                        std::to_string(request_json[JSON_SEQ].asInt());
        req.set_req_id(req_id);
        req.set_amount(request_json[JSON_AMOUNT].asDouble());
        string type = request_json[JSON_TYPE].asString();
        if (type == JSON_QUERY) {
          req.set_type(proto::Request::QUERY);
          req.set_amount(0);
        } else if (type == JSON_DEPOSIT) {
          req.set_type(proto::Request::DEPOSIT);
        } else if (type == JSON_WITHDRAW) {
          req.set_type(proto::Request::WITHDRAW);
        } else if (type == JSON_TRANSFER) {
          req.set_type(proto::Request::TRANSFER);
        } else {
          ifs.close();
          LOG(ERROR) << "The type of the request with req_id=" << req.req_id()
                     << " is illegal" << endl << endl;
          exit(1);
        }
        request_vector.push_back(req);
        LOG_IF(INFO, type == JSON_QUERY)
              << "Request for client " << client.clientid() << ":" << endl
              << "reqid=" << req.req_id() << ", bankid=" << req.bank_id()
              << ", accountid=" << req.account_id() << ", req_type=" << type
              << endl << endl;
        LOG_IF(INFO, type != JSON_QUERY)
              << "Request for client " << client.clientid() << ":" << endl
              << "reqid=" << req.req_id() << ", bankid=" << req.bank_id()
              << ", accountid=" << req.account_id() << ", req_type=" << type
              << ", amount=" << req.amount() << endl << endl;
      }
    } else {  // generate requests randomly
      // from the bank list, randomly choose a bankid
      Json::Value req_seed = client_json[JSON_REQSEED];
      int req_num = req_seed[JSON_REQNUM].asInt();
      int account_num = req_seed[JSON_ACCOUNTNUM].asInt();
      int max_amount = req_seed[JSON_MAXACCOUNT].asInt();
      int prob_query = req_seed[JSON_PROBQUERY].asInt();
      int prob_deposit = req_seed[JSON_PROBDEPOSIT].asInt();
      int prob_withdraw = req_seed[JSON_PROBWITHDRAW].asInt();
      std::map<int, proto::Request_RequestType> genreq_map;
      genreq_map[prob_query] = proto::Request::QUERY;
      genreq_map[prob_query + prob_deposit] = proto::Request::DEPOSIT;
      genreq_map[prob_query + prob_deposit + prob_withdraw] =
          proto::Request::WITHDRAW;
      for (int i = 0; i < req_num; i++) {
        proto::Request req;
        // generate random data
        int bank_seq = rand() % bank_list_json.size();
        string bankid = bank_list_json[bank_seq][JSON_BANKID].asString();
        int account_seq = rand() % account_num;
        string accountid = "account" + std::to_string(account_seq + 1);
        double amount = (double)(rand() % (max_amount + 1));
        int req_tmp = rand() % (prob_query + prob_deposit + prob_withdraw);
        auto it = genreq_map.upper_bound(req_tmp);
        proto::Request_RequestType type = it->second;
        // wrap request object
        req.set_bank_id(bankid);
        req.set_account_id(accountid);
        string req_id =
              bankid + "." + client.clientid() + "." + std::to_string(i + 1);
        req.set_req_id(req_id);
        if (type == proto::Request::QUERY)
          req.set_amount(0);
        else
          req.set_amount(amount);
        req.set_type(type);
        // write log
        std::map<proto::Request_RequestType, string> typestr_map;
        typestr_map[proto::Request::QUERY] = "QUERY";
        typestr_map[proto::Request::DEPOSIT] = "DEPOSIT";
        typestr_map[proto::Request::WITHDRAW] = "WITHDRAW";
        LOG_IF(INFO, type == proto::Request::QUERY)
              << "Request for client " << client.clientid() << ":" << endl
              << "reqid=" << req.req_id() << ", bankid=" << req.bank_id()
              << ", accountid=" << req.account_id()
              << ", req_type=" << typestr_map[type] << endl << endl;
        LOG_IF(INFO, type != proto::Request::QUERY)
              << "Request for client " << client.clientid() << ":" << endl
              << "reqid=" << req.req_id() << ", bankid=" << req.bank_id()
              << ", accountid=" << req.account_id()
              << ", req_type=" << typestr_map[type]
              << ", amount=" << req.amount() << endl << endl;
        request_vector.push_back(req);
      }
    }
    client.set_request_vector(request_vector);
    string drop_interval = root[JSON_CONFIG][JSON_UDP_DROP_INTERVAL].asString();
    if (drop_interval == JSON_RANDOM)
      client.set_drop_interval((rand() % (request_vector.size() - 2) + 2));
    else
      client.set_drop_interval(std::atoi(drop_interval.c_str()));
    LOG(INFO) << "Client " << client.clientid() 
              << ": drop reply interval is " << client.drop_interval() 
              << endl << endl;
    client.set_recv_count(0);
    client_vector.push_back(client);
  }

  ifs.close();
  return 0;
}

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "print help message")(
        "config-file,c", po::value<std::string>(), "specify config file path")(
        "log-dir,l", po::value<std::string>(), "specify logging dir");

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
    }
    FLAGS_logbuflevel = -1;
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "Processing configuration file" << endl << endl;

    vector<Client> client_vector;
    if (read_config_client(vm["config-file"].as<std::string>(), client_vector) != 0)
      return 1;
    
    LOG(INFO) << "Finish processing configuration file. Start the clients."
              << endl << endl;
    vector<std::thread> thread_vector;
    for (auto it = client_vector.begin(); it != client_vector.end(); ++it) {
      Client c = *it;
      thread_vector.push_back(std::thread(c));
    }
    for (auto it = thread_vector.begin(); it != thread_vector.end(); ++it) {
      (*it).join();
    }
  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}
