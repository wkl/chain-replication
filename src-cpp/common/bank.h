#ifndef BANK_H
#define BANK_H

#include <string>
#include <unordered_map>

#include "account.h"

using std::string;
using std::unordered_map;

class Bank
{
public:
  //constructor
  Bank(){};

  // getter/setter
  void set_bankid(string bankid) { bankid_ = bankid; };
  string bankid() { return bankid_; };
  void set_account_map(unordered_map<string,Account> account_map) { account_map_ = account_map; };
  unordered_map<string,Account> account_map() { return account_map_; };
  
private:
  string bankid_;
  unordered_map<string,Account> account_map_;
};

#endif
