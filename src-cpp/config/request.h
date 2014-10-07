#ifndef REQUEST_H
#define REQUEST_H

#include <string>

using namespace std;

class Request
{
public:
  //constructor
  Request(){};

  void set_bankid(string bankid);
  string get_bankid();
  void set_accountid(string accountid);
  string get_accountid();
  void set_seq(int seq);
  int get_seq();
  void set_type(string type);
  string get_type();
  void set_amount(double amount);
  double get_amount();
  
private:
  string bankid;
  string accountid;
  int seq;
  string type;
  double amount;
};

#endif
