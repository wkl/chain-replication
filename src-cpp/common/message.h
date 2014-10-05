/**
 * @file  message.h
 * @brief communication
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>

#include "common.h"

#include "message.pb.h"
namespace pb = google::protobuf;

#include <google/protobuf/io/coded_stream.h>
using pb::io::CodedInputStream;
using pb::io::CodedOutputStream;

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
using google::protobuf::io::ArrayInputStream;
using google::protobuf::io::ArrayOutputStream;

#include <boost/asio.hpp>
namespace asio = boost::asio;
using asio::ip::udp;
using asio::ip::tcp;
using asio::ip::address;

const int MSG_HEADER_SIZE = 4;
const int UDP_MAX_LENGTH = 512;

bool decode_msg(pb::Message &msg, char *buf, uint32_t buf_size);

class UDPLoop {
 public:
  UDPLoop(int port) : port_(port) {}

  void operator()() {
    boost::asio::io_service io_service;
    udp::socket sock(io_service, udp::endpoint(udp::v4(), port_));
    for (;;) {
      char data[UDP_MAX_LENGTH];
      udp::endpoint sender_endpoint;
      size_t length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH),
                                        sender_endpoint);
      Message msg;
      assert(decode_msg(msg, data, length));
      std::cout << "UDP Server received: " << msg.DebugString() << std::endl;

      sock.send_to(asio::buffer(data, length), sender_endpoint);
      handle_msg(msg);
    }
  }

  virtual void handle_msg(Message &msg) = 0;

 private:
  int port_;
};

// prefix serialized message(body) with its size
bool encode_msg(const pb::Message &msg, char *buf, size_t buf_size) {
  assert(buf);
  ArrayOutputStream aos(buf, buf_size);
  CodedOutputStream cos(&aos);
  cos.WriteLittleEndian32(msg.ByteSize());

  return msg.SerializeToCodedStream(&cos);
}

uint32_t decode_hdr(char *buf) {
  assert(buf);
  ArrayInputStream ais(buf, MSG_HEADER_SIZE);
  CodedInputStream cis(&ais);
  uint32_t msg_size;

  if (!cis.ReadLittleEndian32(&msg_size)) return 0;
  // TODO limit max body size

  return msg_size;
}

bool decode_body(pb::Message &msg, char *buf, uint32_t buf_size) {
  assert(buf);
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
bool decode_msg(pb::Message &msg, char *buf, uint32_t buf_size) {
  assert(buf);
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

void prepare_msg(Message &msg, const Message_MessageType &msg_type,
                 const pb::Message &sub_msg) {
  msg.set_type(msg_type);
  switch (msg_type) {
    case Message::REQUEST: {
      Request *tmp = new Request();
      tmp->CopyFrom(sub_msg);          // make a copy
      msg.set_allocated_request(tmp);  // msg takes ownership of tmp
      break;
    }
    default:
      assert(0);  // should not reach here
      break;
  }
}

bool send_msg_tcp(Node target, const Message_MessageType msg_type,
                  const pb::Message &sub_msg) {
  asio::io_service io_service;
  tcp::socket s(io_service);
  tcp::endpoint endpoint(address::from_string(target.ip()), target.port());
  s.connect(endpoint);

  Message msg;
  prepare_msg(msg, msg_type, sub_msg);

  size_t buf_size = msg.ByteSize() + 4;
  char buf[buf_size];
  encode_msg(msg, buf, buf_size);
  std::cout << "Sending TCP message: " << msg.DebugString() << std::endl;
  asio::write(s, asio::buffer(buf, buf_size));

  return true;
}

bool send_msg_udp(Node target, const Message_MessageType msg_type,
                  const pb::Message &sub_msg) {
  asio::io_service io_service;
  udp::socket s(io_service, udp::endpoint(udp::v4(), 0));
  udp::endpoint endpoint(address::from_string(target.ip()), target.port());

  Message msg;
  prepare_msg(msg, msg_type, sub_msg);

  size_t buf_size = msg.ByteSize() + 4;
  assert(buf_size < UDP_MAX_LENGTH);
  char buf[buf_size];
  encode_msg(msg, buf, buf_size);
  std::cout << "Sending UDP message: " << msg.DebugString() << std::endl;
  s.send_to(asio::buffer(buf, buf_size), endpoint);

  // reply test, to be removed
  char reply[buf_size];
  udp::endpoint server_endpoint;
  size_t reply_length =
      s.receive_from(asio::buffer(reply, buf_size), server_endpoint);
  std::cout << "Reply is: ";
  std::cout.write(reply, reply_length);
  std::cout << std::endl;

  return true;
}

#endif
