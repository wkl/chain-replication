#include <string>

#include "bank.h"

using namespace std;

void Bank::set_bankid(string bankid) {
  this->bankid = bankid;
}


string Bank::get_bankid() {
  return this->bankid;
}
