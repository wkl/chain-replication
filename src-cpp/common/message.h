/**
 * @file  message.h
 * @brief communication
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <iostream>
#include <thread>

#include "common.h"
#include "message.pb.h"
namespace pb = google::protobuf;

#include <google/protobuf/io/coded_stream.h>
using pb::io::CodedInputStream;
using pb::io::CodedOutputStream;

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
using pb::io::ArrayInputStream;
using pb::io::ArrayOutputStream;

#include <boost/asio.hpp>
namespace asio = boost::asio;
using asio::ip::udp;
using asio::ip::tcp;
using asio::ip::address;

const int MSG_HEADER_SIZE = 4;
const int UDP_MAX_LENGTH = 512;

bool decode_msg(pb::Message &msg, char *buf, uint32_t buf_size);
uint32_t decode_hdr(char *buf);
bool decode_body(pb::Message &msg, char *buf, uint32_t buf_size);
bool encode_msg(const pb::Message &msg, char *buf, size_t buf_size);
bool send_msg_tcp(proto::Address, const proto::Message_MessageType, const pb::Message &);
bool send_msg_udp(proto::Address, proto::Address, const proto::Message_MessageType, const pb::Message &);
bool msg_udp_loop(proto::Address, proto::Address, const proto::Message_MessageType, const pb::Message &, proto::Message &);

class UDPLoop {
 public:
  UDPLoop(int port) : port_(port) {}

  // thread entry
  void operator()() { run_forever(); }
  void run_forever();

  virtual void handle_msg(proto::Message &msg) = 0;

 private:
  int port_;
};

class TCPLoop {
 public:
  TCPLoop(int port) : port(port) {}

  void operator()() { run_forever(); }
  void run_forever();
  void session(tcp::socket sock);

  virtual void handle_msg(proto::Message &msg) = 0;

 private:
  int port;
};

#endif
