#include <string>
#include <iostream>
#include <fstream>

#include "json/json.h"
#include "master.h"
#include "server.h"
#include "client.h"
#include "bank.h"
#include "request.h"

using namespace std;

int main() {
  //string test ="{\"id\":1,\"name\":\"kurama\"}";
  Json::Reader reader;
  Json::Value root;

  ifstream is;
  is.open("configuration.json", ios::binary);

  if(reader.parse(is,root)) {
    Json::Value master_json;
    Master master;
    master_json = root["master"];
    master.set_ip(master_json["ip"].asString());
    master.set_port(master_json["port"].asInt());
    cout<<"master ip:"<<master.get_ip()<<" "<<"master port:"<<master.get_port()<<endl;
    cout<<endl;

    Json::Value bank_list_json;
    bank_list_json = root["bank"];
    for(int i = 0; i < bank_list_json.size(); i++) {
      Json::Value bank_json = bank_list_json[i];
      Bank bank;
      bank.set_bankid(bank_json["bankid"].asString());
      cout<<"bankid:"<<bank.get_bankid()<<endl;
      Json::Value server_list_json;
      server_list_json = bank_json["servers"];
      for (int j = 0; j < server_list_json.size(); j++) {
        Json::Value server_json = server_list_json[j];
        Server server;
        server.set_bankid(bank.get_bankid());
	server.set_serverid(server_json["serverid"].asString());
	server.set_ip(server_json["ip"].asString());
	server.set_port(server_json["port"].asInt());
	server.set_chain_no(server_json["chainno"].asInt());
	server.set_start_delay(server_json["startdelay"].asInt());
	server.set_life_time(server_json["lifetime"].asString());
	cout<<"    "<<"server id:"<<server.get_serverid()<<" bankid:"<<server.get_bankid()
	            <<" ip:"<<server.get_ip()<<" port:"<<server.get_port()
                    <<" chain_no:"<<server.get_chain_no()<<" start_delay:"<<server.get_start_delay()
                    <<" life_time:"<<server.get_life_time()<<endl;
      }
    }
    cout<<endl;

    Json::Value client_list_json;
    client_list_json = root["client"];
    for(int i = 0; i < client_list_json.size(); i++) {
      Json::Value client_json = client_list_json[i];
      Client client;
      client.set_clientid(client_json["clientid"].asString());
      client.set_ip(client_json["ip"].asString());
      client.set_port(client_json["port"].asInt());
      client.set_wait_timeout(client_json["waittimeout"].asInt());
      client.set_resend_num(client_json["resendnum"].asInt());
      client.set_if_resend(client_json["ifresend"].asBool());
      cout<<"client id:"<<client.get_clientid()<<" ip:"<<client.get_ip()<<" port:"<<client.get_port()
          <<" wait_timeout:"<<client.get_wait_timeout()<<" resend_num:"<<client.get_resend_num()
          <<" if_resend:"<<client.get_if_resend()<<endl;

      Json::Value request_json_list;
      request_json_list = client_json["request"];
      for (int j = 0; j < request_json_list.size(); j++) {
        Json::Value request_json = request_json_list[j];
        Request request;
	request.set_bankid(request_json["bankid"].asString());
	request.set_accountid(request_json["accountid"].asString());
	request.set_seq(request_json["seq"].asInt());
	request.set_type(request_json["type"].asString());
	request.set_amount(request_json["amount"].asDouble());
	cout<<"    "<<"request bankid:"<<request.get_bankid()<<" accountid:"<<request.get_accountid()
                    <<" seq:"<<request.get_seq()<<" type:"<<request.get_type()<<" amount"<<request.get_amount()<<endl;
      }
      cout<<endl;
    }
  }

  is.close();
  return 0;
}

