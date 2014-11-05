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
#define JSON_CONFIG "config"
#define JSON_SERVER_REPORT_INTERVAL "server_report_interval"
#define JSON_SERVER_FAIL_TIMEOUT "server_fail_timeout"
#define JSON_MASTER "master"
#define JSON_EXTEND_SERVERS "extend_servers"
#define JSON_START_DEALY "startdelay"
#define JSON_FAIL_SCENARIO "failscenario"
#define JSON_FAIL_SEQ "failseq"
#define JSON_NONE "none"
#define JSON_FAIL_AFTER_SEND "fail_after_send"
#define JSON_FAIL_AFTER_RECV "fail_after_recv"
#define JSON_FAIL_AFTER_SEND_IN_EXTEND "fail_after_send_in_extend"
#define JSON_FAIL_AFTER_RECV_IN_EXTEND "fail_after_recv_in_extend"
#define JSON_FAIL_SEQ "failseq"

// global
extern int send_msg_seq;
extern int rec_msg_seq;

#endif
