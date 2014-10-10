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
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void Client::receive_reply(const proto::Reply& reply) {
  std::cout << "Reply " << reply.req_id() << " received." << std::endl;
}

void Client::run() {
  // ugly, may refactor with UDPLoop
  boost::asio::io_service io_service;
  udp::socket sock(io_service, udp::endpoint(udp::v4(), port_)); // receiver
  char data[UDP_MAX_LENGTH];
  udp::endpoint sender_endpoint;
  size_t length;

  try {
    for(auto it=request_vector_.begin(); it!=request_vector_.end(); ++it) {
      proto::Request req = *it;  
      // client address
      proto::Address* addr = new proto::Address;
      addr->set_ip(ip_);
      addr->set_port(port_);
      req.set_allocated_client_addr(addr);
      // check bank
      string bankid = req.bank_id();
      auto it_head = bank_head_list_.find(bankid);
      assert(it_head != bank_head_list_.end());
      proto::Address head_addr = (*it_head).second;
      auto it_tail = bank_tail_list_.find(bankid);
      assert(it_tail != bank_tail_list_.end());
      proto::Address tail_addr = (*it_tail).second;
      // check request type and send request
      if (req.type() == proto::Request::QUERY) {
	send_msg_udp(tail_addr, proto::Message::REQUEST, req);
      } else {
        send_msg_udp(head_addr, proto::Message::REQUEST, req);
      }
      // receive
      length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
      proto::Message msg;
      assert(decode_msg(msg, data, length));
      std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
      handle_msg(msg);
    }
  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
}

// read configuration file
int read_config_client(string dir, vector<Client>& client_vector) {
  Json::Reader reader;
  Json::Value root;
  cout<<dir<<endl;
  std::ifstream is;
  is.open(dir, std::ios::binary);
  if (!is.is_open()) { 
    cout << "Error opening config file "<<dir<<endl; 
    exit (1); 
  } 
  if(reader.parse(is,root)) {
    Json::Value client_list_json;
    client_list_json = root[JSON_CLIENTS];
    for(int i = 0; i < client_list_json.size(); i++) {
      Json::Value client_json = client_list_json[i];
      Client client;
      client.set_clientid(client_json[JSON_CLIENTID].asString());
      client.set_ip(client_json[JSON_IP].asString());
      client.set_port(client_json[JSON_PORT].asInt());
      client.set_wait_timeout(client_json[JSON_WAIT_TIMEOUT].asInt());
      client.set_resend_num(client_json[JSON_RESEND_NUM].asInt());
      client.set_if_resend(client_json[JSON_IF_RESEND].asBool());
      cout<<"client id:"<<client.clientid()<<" ip:"<<client.ip()<<" port:"<<client.port()
          <<" wait_timeout:"<<client.wait_timeout()<<" resend_num:"<<client.resend_num()
          <<" if_resend:"<<client.if_resend()<<endl;

      vector<proto::Request> request_vector;
      Json::Value request_json_list;
      request_json_list = client_json[JSON_REQUESTS];
      for (int j = 0; j < request_json_list.size(); j++) {
        Json::Value request_json = request_json_list[j];
        proto::Request req;
	req.set_bank_id(request_json[JSON_BANKID].asString());
	req.set_account_id(request_json[JSON_ACCOUNTID].asString());
	req.set_read_seq(request_json[JSON_SEQ].asInt());
	string req_id = req.bank_id() + "." + req.account_id() + "." + std::to_string(req.read_seq());
	req.set_req_id(req_id);
	req.set_amount(request_json[JSON_AMOUNT].asDouble());
        string type = request_json[JSON_TYPE].asString();
	if (type == JSON_QUERY) {
	  req.set_type(proto::Request::QUERY);
	} else if (type == JSON_DEPOSIT) {
	  req.set_type(proto::Request::DEPOSIT);
        } else if (type == JSON_WITHDRAW) {
	  req.set_type(proto::Request::WITHDRAW);
	} else if (type == JSON_TRANSFER) {
	  req.set_type(proto::Request::TRANSFER);
	} else {
	  is.close();
    	  cout << "Illegal request type."<<endl; 
    	  exit (1);
	}
	request_vector.push_back(req);
	cout<<"    "<<"request reqid: "<<req.req_id()<<", bankid: "<<req.bank_id()<<", accountid: "<<req.account_id()
                    <<", seq: "<<req.read_seq()<<", type: "<<req.type()<<", amount: "<<req.amount()<<endl;
      }
      client.set_request_vector(request_vector); 

      Json::Value bank_list_json;
      bank_list_json = root[JSON_BANKS];
      for(int i = 0; i < bank_list_json.size(); i++) {
        Json::Value bank_json = bank_list_json[i];
        Json::Value server_list_json;
        server_list_json = bank_json[JSON_SERVERS];
	proto::Address head_addr;  // head server address
        head_addr.set_ip(server_list_json[0][JSON_IP].asString());
        head_addr.set_port(server_list_json[0][JSON_PORT].asInt());
	proto::Address tail_addr;  // tail server address
        tail_addr.set_ip(server_list_json[server_list_json.size()-1][JSON_IP].asString());
        tail_addr.set_port(server_list_json[server_list_json.size()-1][JSON_PORT].asInt());
        auto it_head = client.bank_head_list().insert(std::make_pair(bank_json[JSON_BANKID].asString(), head_addr));
    	assert(it_head.second);
	auto it_tail = client.bank_tail_list().insert(std::make_pair(bank_json[JSON_BANKID].asString(), tail_addr));
    	assert(it_tail.second);
      }
      
      client_vector.push_back(client);
      cout<<endl;    
    }
  }
  else {
    is.close();
    cout << "Error parsing config file "<<dir<<endl; 
    exit (1); 
  }

  is.close();
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

    vector<Client> client_vector;
    read_config_client(vm["config-file"].as<std::string>(), client_vector);

    for(auto it=client_vector.begin(); it!=client_vector.end(); ++it) {
      Client c = *it;
      std::thread t(c);
      t.join();
    }

  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }
  /*
  Client c("127.0.0.1", 60001);	// client address
  std::thread t(c);
  t.join();
  */

  return 0;
}

