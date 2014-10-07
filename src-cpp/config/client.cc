#include <string>

#include "client.h"

void Client::set_clientid(string clientid) {
  this->clientid = clientid;
}


string Client::get_clientid() {
  return this->clientid;
}


void Client::set_wait_timeout(int wait_timeout) {
  this->wait_timeout = wait_timeout;
}


int Client::get_wait_timeout() {
  return this->wait_timeout;
}


void Client::set_resend_num(int resend_num) {
  this->resend_num = resend_num;
}


int Client::get_resend_num() {
  return this->resend_num;
}


void Client::set_if_resend(bool if_resend) {
  this->if_resend = if_resend;
}


bool Client::get_if_resend() {
  return this->if_resend;
}
