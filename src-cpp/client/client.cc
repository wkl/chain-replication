#include "client.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

void Client::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REPLY:
      assert(msg.has_reply());
      receive_reply(msg.reply());
      break;
    default:
      LOG(ERROR) << "no handler for message type (" << msg.type() << ")" << endl
                 << endl;
      break;
  }
}

// client deal with reply
void Client::receive_reply(const proto::Reply& reply) {}

void Client::run() {
  // ugly, may refactor with UDPLoop
  boost::asio::io_service io_service;
  udp::socket sock(io_service, udp::endpoint(udp::v4(), port_));  // receiver
  char data[UDP_MAX_LENGTH];
  udp::endpoint sender_endpoint;
  size_t length;

  try {
    for (auto it = request_vector_.begin(); it != request_vector_.end(); ++it) {
      proto::Request req = *it;
      // client address
      proto::Address* local_addr = new proto::Address;
      local_addr->set_ip(ip_);
      local_addr->set_port(port_);
      req.set_allocated_client_addr(local_addr);
      // bank server address
      string bankid = req.bank_id();
      auto it_head = bank_head_list_.find(bankid);
      assert(it_head != bank_head_list_.end());
      proto::Address head_addr = (*it_head).second;
      auto it_tail = bank_tail_list_.find(bankid);
      assert(it_tail != bank_tail_list_.end());
      proto::Address tail_addr = (*it_tail).second;

      // check request type and send request
      if (req.type() == proto::Request::QUERY) {
        send_msg_seq++;
        send_msg_udp(*local_addr, tail_addr, proto::Message::REQUEST, req);
        LOG(INFO) << "Client " << clientid_ << " sent udp message to "
                  << tail_addr.ip() << ":" << tail_addr.port()
                  << ", send_req_seq = " << send_msg_seq << endl
                  << req.ShortDebugString() << endl << endl;
      } else {
        send_msg_seq++;
        send_msg_udp(*local_addr, head_addr, proto::Message::REQUEST, req);
        LOG(INFO) << "Client " << clientid_ << " sent udp message to "
                  << head_addr.ip() << ":" << head_addr.port()
                  << ", send_req_seq = " << send_msg_seq << endl
                  << req.ShortDebugString() << endl << endl;
      }
      // receive
      length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH),
                                 sender_endpoint);
      proto::Message msg;
      assert(decode_msg(msg, data, length));
      rec_msg_seq++;
      LOG(INFO) << "Client " << clientid_ << " received udp message from "
                << sender_endpoint << ", rec_req_seq = " << rec_msg_seq << endl
                << msg.ShortDebugString() << endl << endl;
      handle_msg(msg);
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
    exit(1);
  }
  if (reader.parse(ifs, root)) {
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
      client.set_if_resend(client_json[JSON_IF_RESEND].asBool());
      LOG(INFO) << "New client initialization: "
                << " clientid=" << client.clientid() << ", ip=" << client.ip()
                << ", port=" << client.port()
                << ", wait_timeout=" << client.wait_timeout()
                << ", resend_num=" << client.resend_num() << endl << endl;
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
      if (!client_json.isMember("reqseed")) {  // read requests from file
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
        Json::Value req_seed = client_json["reqseed"];
        int req_num = req_seed["reqnum"].asInt();
        int account_num = req_seed["accountnum"].asInt();
        int max_amount = req_seed["maxamount"].asInt();
        int prob_query = req_seed["probquery"].asInt();
        int prob_deposit = req_seed["probdeposit"].asInt();
        int prob_withdraw = req_seed["probwithdraw"].asInt();
        std::map<int, proto::Request_RequestType> genreq_map;
        genreq_map[prob_query] = proto::Request::QUERY;
        genreq_map[prob_query + prob_deposit] = proto::Request::DEPOSIT;
        genreq_map[prob_query + prob_deposit + prob_withdraw] =
            proto::Request::WITHDRAW;
        srand((unsigned)time(0));
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

      client_vector.push_back(client);
    }
  } else {
    ifs.close();
    LOG(ERROR) << "Fail to parse the config file: " << dir << endl << endl;
    exit(1);
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

    FLAGS_logtostderr = true;
    if (vm.count("log-dir")) {
      FLAGS_log_dir = vm["log-dir"].as<string>();
      FLAGS_logtostderr = false;
    }
    FLAGS_logbuflevel = -1;
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "Processing configuration file" << endl << endl;

    if (vm.count("config-file")) {
      vector<Client> client_vector;
      read_config_client(vm["config-file"].as<std::string>(), client_vector);
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
    } else {
      LOG(ERROR) << "Please input the config-file path" << endl << endl;
      return 1;
    }
  } catch (std::exception& e) {
    LOG(ERROR) << "error: " << e.what() << endl << endl;
    return 1;
  }

  return 0;
}

