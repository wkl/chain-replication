#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>

using std::string;

class Account
{
public:
  // constructor
  Account(){ balance_ = 0; };
  Account(string accountid, float balance) : accountid_(accountid), balance_(balance) {};
  
  // getter/setter
  void set_accountid(string accountid) { accountid_ = accountid; };
  string accountid() { return accountid_; };
  void set_balance(float balance) { balance_ = balance; };
  float balance() { return balance_; };
  
private:
  string accountid_;
  float balance_;
};

#endif
