#include <string>

#include "request.h"

using namespace std;

void Request::set_bankid(string bankid) {
  this->bankid = bankid;
}


string Request::get_bankid() {
  return this->bankid;
}


void Request::set_accountid(string accountid) {
  this->accountid = accountid;
}


string Request::get_accountid() {
  return this->accountid;
}


void Request::set_seq(int seq) {
  this->seq = seq;
}


int Request::get_seq() {
  return this->seq;
}


void Request::set_type(string type) {
  this->type = type;
}


string Request::get_type() {
  return this->type;
}


void Request::set_amount(double amount) {
  this->amount = amount;
}


double Request::get_amount() {
  return this->amount;
}
