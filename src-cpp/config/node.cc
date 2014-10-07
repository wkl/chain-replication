#include <string>

#include "node.h"

using namespace std;

Node::Node(string ip, int port) {
  this->ip = ip;
  this->port = port;
}

void Node::set_ip(string ip) {
  this->ip = ip;
}


string Node::get_ip() {
  return this->ip;
}

void Node::set_port(int port) {
  this->port = port;
}


int Node::get_port() {
  return this->port;
}
