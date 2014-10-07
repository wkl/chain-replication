#ifndef COMMON_H
#define COMMON_H

#include <string>

#include <json/json.h>
#include <glog/logging.h>

class Node {
 public:
  Node(std::string ip, unsigned short port) : ip_(ip), port_(port) {}
  const std::string &ip() { return ip_; }
  unsigned short port() { return port_; }

 private:
  std::string ip_;
  unsigned short port_;
};

#endif
