#ifndef COMMON_H
#define COMMON_H

#include <string>

#include <json/json.h>
#include <glog/logging.h>

#define JSON_IP "ip"
#define JSON_PORT "port"
#define JSON_CLIENTS "clients"
#define JSON_CLIENTID "clientid"
#define JSON_WAIT_TIMEOUT "waittimeout"
#define JSON_RESEND_NUM "resendnum"
#define JSON_IF_RESEND "ifresend"
#define JSON_REQUESTS "requests"
#define JSON_QUERY "QUERY"
#define JSON_DEPOSIT "DEPOSIT"
#define JSON_WITHDRAW "WITHDRAW"
#define JSON_TRANSFER "TRANSFER"
#define JSON_BANKID "bankid"
#define JSON_ACCOUNTID "accountid"
#define JSON_SEQ "seq"
#define JSON_TYPE "type"
#define JSON_AMOUNT "amount"
#define JSON_BANKS "banks"
#define JSON_SERVERS "servers"
#define JSON_CHAINNO "chainno"

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
