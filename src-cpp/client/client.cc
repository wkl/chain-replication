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
    // test 0
    proto::Request req;
    req.set_type(proto::Request::DEPOSIT);
    req.set_req_id("bank1.client1.1");
    req.set_account_id("client1");
    req.set_bank_id("bank1");
    req.set_amount(100);
    
    // client address
    proto::Address* addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);

    //Node head("127.0.0.1", 50001);
    proto::Address head;	// head server address
    head.set_ip("127.0.0.1");
    head.set_port(50001);
    //Node tail("127.0.0.1", 50003);
    proto::Address tail;	// tail server address
    tail.set_ip("127.0.0.1");
    tail.set_port(50003);

    send_msg_udp(head, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    proto::Message msg;
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);

    // test 1
    req.set_type(proto::Request::WITHDRAW);
    req.set_req_id("bank1.client1.2");
    req.set_account_id("client1");
    req.set_bank_id("bank1");
    req.set_amount(200);
    addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);
    send_msg_udp(head, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);

    // test 2
    req.set_type(proto::Request::WITHDRAW);
    req.set_req_id("bank1.client1.3");
    req.set_account_id("client1");
    req.set_bank_id("bank1");
    req.set_amount(20);
    addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);
    send_msg_udp(head, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);

    // test 3
    req.set_type(proto::Request::WITHDRAW);
    req.set_req_id("bank1.client1.3");
    req.set_account_id("client1");
    req.set_bank_id("bank1");
    req.set_amount(200);
    addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);
    send_msg_udp(head, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);

    // test 4
    req.set_type(proto::Request::QUERY);
    req.set_req_id("bank1.client1.4");
    req.set_account_id("client1");
    req.set_bank_id("bank1");
    //req.set_amount(200);
    addr = new proto::Address;
    addr->set_ip(ip_);
    addr->set_port(port_);
    req.set_allocated_client_addr(addr);
    send_msg_udp(tail, proto::Message::REQUEST, req);

    length = sock.receive_from(asio::buffer(data, UDP_MAX_LENGTH), sender_endpoint);
    assert(decode_msg(msg, data, length));
    std::cout << "UDP message Received from: " << sender_endpoint << std::endl
        << msg.ShortDebugString() << std::endl;
    handle_msg(msg);
 
  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
  }
}

int main(int argc, char* argv[]) {
  Client c("127.0.0.1", 60001);	// client address
  std::thread t(c);
  t.join();
  return 0;
}

