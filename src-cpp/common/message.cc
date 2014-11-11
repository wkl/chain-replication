#include "message.h"

// global
int send_msg_seq = 0;
int rec_msg_seq = 0;

void UDPLoop::run_forever() {
  boost::asio::io_service io_service;
  udp::socket sock(io_service, udp::endpoint(udp::v4(), port_));
  for (;;) {
    char data[UDP_MAX_LENGTH];
    udp::endpoint sender_endpoint;
    size_t length =
        sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    proto::Message msg;
    assert(decode_msg(msg, data, length));

    /*
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
              << msg.ShortDebugString() << std::endl;
    */

    // decode from_endpoint
    proto::Address from_addr;
    from_addr.set_ip(sender_endpoint.address().to_string());
    from_addr.set_port(sender_endpoint.port());

    handle_msg(msg, from_addr);
  }
}

void TCPLoop::run_forever() {
  boost::asio::io_service io_service;
  tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
  for (;;) {
    tcp::socket sock(io_service);
    a.accept(sock);
    // std::thread(&TCPLoop::session, this, std::move(sock)).detach();
    session(std::move(sock));
  }
}

void TCPLoop::session(tcp::socket sock) {
  try {
    // read header (the body size)
    char header[MSG_HEADER_SIZE];
    boost::system::error_code error;
    size_t length =
        asio::read(sock, asio::buffer(header, MSG_HEADER_SIZE), error);
    if (error == asio::error::eof)
      return;  // Connection closed cleanly by peer.
    else if (error)
      throw boost::system::system_error(error);  // Some other error.
    assert(length == MSG_HEADER_SIZE);

    // read body
    size_t body_size = decode_hdr(header);
    char body[body_size];
    length = asio::read(sock, asio::buffer(body, body_size), error);
    if (error == asio::error::eof)
      return;  // Connection closed cleanly by peer.
    else if (error)
      throw boost::system::system_error(error);  // Some other error.
    assert(length == body_size);

    // decode msg
    proto::Message msg;
    decode_body(msg, body, body_size);
    /*
    std::cout << "TCP message received from: " << sock.remote_endpoint()
              << std::endl << msg.ShortDebugString() << std::endl;
    */

    // decode from_endpoint
    proto::Address from_addr;
    from_addr.set_ip(sock.remote_endpoint().address().to_string());
    from_addr.set_port(sock.remote_endpoint().port());

    handle_msg(msg, from_addr);
  } catch (std::exception &e) {
    std::cerr << "Exception in thread: " << e.what() << std::endl;
  }
}

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

void prepare_msg(proto::Message &msg,
                 const proto::Message_MessageType &msg_type,
                 const pb::Message &sub_msg) {
  msg.set_type(msg_type);
  switch (msg_type) {
    case proto::Message::REQUEST: {
      auto *tmp = new proto::Request();
      tmp->CopyFrom(sub_msg);          // make a copy
      msg.set_allocated_request(tmp);  // msg takes ownership of tmp
      break;
    }
    case proto::Message::REPLY: {
      auto *tmp = new proto::Reply();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_reply(tmp);
      break;
    }
    case proto::Message::ACKNOWLEDGE: {
      auto *tmp = new proto::Acknowledge();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_ack(tmp);
      break;
    }
    case proto::Message::HEARTBEAT: {
      auto *tmp = new proto::Heartbeat();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_heartbeat(tmp);
      break;
    }
    case proto::Message::TO_BE_HEAD: {
      break;
    }
    case proto::Message::NEW_HEAD: {
      auto *tmp = new proto::Notify();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_notify(tmp);
      break;
    }
    case proto::Message::TO_BE_TAIL: {
      break;
    }
    case proto::Message::NEW_TAIL: {
      auto *tmp = new proto::Notify();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_notify(tmp);
      break;
    }
    case proto::Message::NEW_PRE_SERVER: {
      auto *tmp = new proto::Address();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_addr(tmp);
      break;
    }
    case proto::Message::REQ_SEQ: {
      auto *tmp = new proto::Reqseq();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_reqseq(tmp);
      break;
    }
    case proto::Message::NEW_SUCC_SERVER: {
      auto *tmp = new proto::Reqseq();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_reqseq(tmp);
      break;
    }
    case proto::Message::NEW_TAIL_READY: {
      auto *tmp = new proto::Address();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_addr(tmp);
      break;
    }
    case proto::Message::JOIN: {
      auto *tmp = new proto::Join();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_join(tmp);
      break;
    }
    case proto::Message::EXTEND_SERVER: {
      auto *tmp = new proto::Address();
      tmp->CopyFrom(sub_msg);
      msg.set_allocated_addr(tmp);
      break;
    }
    default:
      LOG(ERROR) << "Unknown msg type";
      assert(0);  // should not reach here
      break;
  }
}

/**
 * @brief send message with tcp
 *
 * @param[in]   target    destination
 * @param[in]   msg_type  message type
 * @param[out]  sub_msg   payload of message
 *
 * @return true on success.
 */
bool send_msg_tcp(proto::Address target,
                  const proto::Message_MessageType msg_type,
                  const pb::Message &sub_msg) {
  asio::io_service io_service;
  boost::system::error_code ec;
  tcp::socket s(io_service);
  tcp::endpoint endpoint(address::from_string(target.ip()), target.port());

  s.connect(endpoint, ec);
  if (ec) {
    LOG(INFO) << "Fail to connect to " << endpoint << " to send tcp message" << endl << endl;
    return false;
  }

  proto::Message msg;
  prepare_msg(msg, msg_type, sub_msg);

  size_t buf_size = msg.ByteSize() + 4;
  char buf[buf_size];
  encode_msg(msg, buf, buf_size);
  /*
  std::cout << "Sending TCP message..."
            << " from: " << s.local_endpoint() << " to: " << s.remote_endpoint()
            << std::endl << sub_msg.ShortDebugString() << std::endl;
  */
  asio::write(s, asio::buffer(buf, buf_size), ec);
  if (ec) {
    LOG(INFO) << "Fail to write data to " << endpoint << " to send tcp message" << endl << endl;
    return false;
  }

  s.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
  s.close();
  return true;
}

bool send_msg_udp(proto::Address local, proto::Address target,
                  const proto::Message_MessageType msg_type,
                  const pb::Message &sub_msg) {
  asio::io_service io_service;
  udp::socket s(io_service, udp::endpoint(address::from_string(local.ip()), 0));
  udp::endpoint endpoint(address::from_string(target.ip()), target.port());

  proto::Message msg;
  prepare_msg(msg, msg_type, sub_msg);

  size_t buf_size = msg.ByteSize() + 4;
  assert(buf_size < UDP_MAX_LENGTH);
  char buf[buf_size];
  encode_msg(msg, buf, buf_size);
  /*
  LOG(INFO) << "Sending UDP message..."
            << " from: " << s.local_endpoint() << " to: " << endpoint
            << std::endl << msg.ShortDebugString();
  */
  s.send_to(asio::buffer(buf, buf_size), endpoint);

  return true;
}

// deprecated
bool msg_udp_loop(proto::Address local, proto::Address target,
                  const proto::Message_MessageType msg_type,
                  const pb::Message &sub_msg, proto::Message &rec_msg) {
  asio::io_service io_service;
  udp::endpoint local_endpoint(address::from_string(local.ip()), local.port());
  udp::socket sock(io_service, local_endpoint);
  // send
  udp::endpoint dest_endpoint(address::from_string(target.ip()), target.port());
  proto::Message send_msg;
  prepare_msg(send_msg, msg_type, sub_msg);

  size_t buf_size = send_msg.ByteSize() + 4;
  assert(buf_size < UDP_MAX_LENGTH);
  char buf[buf_size];
  encode_msg(send_msg, buf, buf_size);
  LOG(INFO) << "Sending UDP message..."
            << " from: " << sock.local_endpoint() << " to: " << dest_endpoint
            << std::endl << send_msg.ShortDebugString();
  sock.send_to(asio::buffer(buf, buf_size), dest_endpoint);

  // receive
  char data[UDP_MAX_LENGTH];
  udp::endpoint sender_endpoint;
  size_t length;

  length =
      sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
  assert(decode_msg(rec_msg, data, length));
  LOG(INFO) << "UDP message Received from: " << sender_endpoint << std::endl
            << rec_msg.ShortDebugString();
  return true;
}

