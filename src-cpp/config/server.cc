#include <string>

#include "server.h"

using namespace std;

void Server::set_bankid(string bankid) {
  this->bankid = bankid;
}


string Server::get_bankid() {
  return this->bankid;
}


void Server::set_serverid(string serverid) {
  this->serverid = serverid;
}


string Server::get_serverid() {
  return this->serverid;
}


void Server::set_chain_no(int chain_no) {
  this->chain_no = chain_no;
}


int Server::get_chain_no() {
  return chain_no;
}


void Server::set_start_delay(int start_delay) {
  this->start_delay = start_delay;
}


int Server::get_start_delay() {
  return this->start_delay;
}


void Server::set_life_time(string life_time) {
  this->life_time = life_time;
}


string Server::get_life_time() {
  return this->life_time;
}


void Server::set_ishead(bool ishead) {
  this->ishead = ishead;
}


bool Server::get_ishead() {
  return this->ishead;
}


void Server::set_istail(bool istail) {
  this->istail = istail;
}


bool Server::get_istail() {
  return this->istail;
}


void Server::set_extending_chain(bool extending_chain) {
  this->extending_chain = extending_chain;
}


bool Server::get_extending_chain() {
  return this->extending_chain;
}
