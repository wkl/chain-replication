#ifndef COMMON_H
#define COMMON_H

#include <string>

#include "message.pb.h"

#include <google/protobuf/io/coded_stream.h>
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::CodedOutputStream;

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
using google::protobuf::io::ArrayInputStream;
using google::protobuf::io::ArrayOutputStream;

#include <boost/asio.hpp>
using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::address;

const uint32_t MSG_HEADER_SIZE = 4;

class Node {
 public:
  Node(std::string ip, unsigned short port) : ip_(ip), port_(port) {}
  const std::string &ip() { return ip_; }
  unsigned short port() { return port_; }

 private:
  std::string ip_;
  unsigned short port_;
};

// prefix serialized message(body) with its size
bool encode_msg(const google::protobuf::Message &msg, char *buf,
                size_t buf_size) {
  assert(buf);
  ArrayOutputStream aos(buf, buf_size);
  CodedOutputStream cos(&aos);
  cos.WriteLittleEndian32(msg.ByteSize());

  return msg.SerializeToCodedStream(&cos);
}

uint32_t decode_hdr(char *buf) {
  ArrayInputStream ais(buf, MSG_HEADER_SIZE);
  CodedInputStream cis(&ais);
  uint32_t msg_size;

  if (!cis.ReadLittleEndian32(&msg_size)) return 0;
  // TODO limit max body size

  return msg_size;
}

bool decode_body(google::protobuf::Message &msg, char *buf, uint32_t buf_size) {
  ArrayInputStream ais(buf, buf_size);
  CodedInputStream cis(&ais);

  cis.PushLimit(buf_size);
  return msg.ParseFromCodedStream(&cis);
}

/**
 * @brief Decode msg (header + body), currently used in UDP
 *
 * @param[out]  msg       Protobuf instance
 * @param[in]   buf       msg buffer
 * @param[out]  buf_size  Buffer size
 *
 * @return true on success.
 */
bool decode_msg(google::protobuf::Message &msg, char *buf, uint32_t buf_size) {
  ArrayInputStream ais(buf, buf_size);
  CodedInputStream cis(&ais);
  uint32_t msg_size;

  if (!cis.ReadLittleEndian32(&msg_size)) return false;
  // assert client doesn't send malformed msg
  assert(buf_size == msg_size + MSG_HEADER_SIZE);
  if (buf_size < msg_size + MSG_HEADER_SIZE) return false;

  cis.PushLimit(msg_size);
  return msg.ParseFromCodedStream(&cis);
}

bool send_msg_tcp(Node target, Request &req) {
  boost::asio::io_service io_service;
  tcp::socket s(io_service);
  tcp::endpoint endpoint(address::from_string(target.ip()), target.port());
  s.connect(endpoint);

  size_t buf_size = req.ByteSize() + 4;
  char buf[buf_size];
  encode_msg(req, buf, buf_size);
  std::cout << "Sending TCP message..." << std::endl;
  boost::asio::write(s, boost::asio::buffer(buf, buf_size));

  return true;
}

bool send_msg_udp(Node target, Request &req) {
  boost::asio::io_service io_service;
  udp::socket s(io_service, udp::endpoint(udp::v4(), 0));
  udp::endpoint endpoint(address::from_string(target.ip()), target.port());

  size_t buf_size = req.ByteSize() + 4;
  assert(buf_size < 512);
  char buf[buf_size];
  encode_msg(req, buf, buf_size);
  std::cout << "Sending UDP message..." << std::endl;
  s.send_to(boost::asio::buffer(buf, buf_size), endpoint);

  // reply test
  char reply[buf_size];
  udp::endpoint server_endpoint;
  size_t reply_length =
      s.receive_from(boost::asio::buffer(reply, buf_size), server_endpoint);
  std::cout << "Reply is: ";
  std::cout.write(reply, reply_length);
  std::cout << std::endl;

  return true;
}
#endif
