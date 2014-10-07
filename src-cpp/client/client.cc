#include "client.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

void Client::handle_msg(proto::Message& msg) {
  switch (msg.type()) {
    case proto::Message::REPLY:
      assert(msg.has_reply());
      receive_reply(msg.reply());
      break;
    default:
      std::cerr << "no handler for message type (" << msg.type() << ")"
                << std::endl;
      break;
  }
}

void Client::receive_reply(const proto::Reply& reply) {
  std::cout << "Reply " << reply.req_id() << " received." << std::endl;
}

void Client::run() {
  // ugly, may refactor with UDPLoop
  boost::asio::io_service io_service;
  udp::socket sock(io_service, udp::endpoint(udp::v4(), port_)); // receiver
  char data[UDP_MAX_LENGTH];
  udp::endpoint sender_endpoint;
  size_t length;

  try {
    proto::Request req;
    req.set_type(proto::Request::QUERY);
    req.set_req_id("boa.1.1");
    proto::Address* addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);

    Node head("127.0.0.1", 50001);
    send_msg_udp(head, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    proto::Message msg;
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);
  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
}

int main(int argc, char* argv[]) {
  Client c("127.0.0.1", 60001);
  std::thread t(c);
  t.join();
  return 0;
}

