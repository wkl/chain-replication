#ifndef BANK_H
#define BANK_H

#include <string>

using namespace std;

class Bank
{
public:
  //constructor
  Bank(){};

  void set_bankid(string bankid);
  string get_bankid();
  
private:
  string bankid;
};

#endif
