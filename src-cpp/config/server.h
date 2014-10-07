#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "node.h"
#include "bank.h"

using namespace std;

class Server : public Node
{
public:
  //constructor
  Server(){};
  Server(string ip, int port)
	:Node(ip, port){};
	
  void set_bankid(string bankid);
  string get_bankid();
  void set_serverid(string serverid);
  string get_serverid();
  void set_chain_no(int chain_no);
  int get_chain_no();
  void set_start_delay(int start_delay);
  int get_start_delay();
  void set_life_time(string life_time);
  string get_life_time();
  void set_ishead(bool ishead);
  bool get_ishead();
  void set_istail(bool istail);
  bool get_istail();
  void set_extending_chain(bool extending_chain);
  bool get_extending_chain();
  
private:
  string bankid;
  string serverid;
  int chain_no;
  int start_delay;
  string life_time;
  bool ishead;
  bool istail;
  bool extending_chain;    
};

#endif
