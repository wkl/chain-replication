#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "node.h"

using namespace std;

class Client : public Node
{
public:
  //constructor
  Client(){};
  
  void set_clientid(string clientid);
  string get_clientid();
  void set_wait_timeout(int wait_timeout);
  int get_wait_timeout();
  void set_resend_num(int resend_num);
  int get_resend_num();
  void set_if_resend(bool if_resend);
  bool get_if_resend();
  
private:
  string clientid;
  int wait_timeout;
  int resend_num;
  bool if_resend;
};

#endif

