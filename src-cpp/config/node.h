#ifndef NODE_H
#define NODE_H

#include <string>

using namespace std;

class Node
{
public:
  //constructor
  Node(){};
  Node(string ip, int port);	

  void set_ip(string ip);
  string get_ip();
  void set_port(int port);
  int get_port();
  
private:
  string ip;
  int port;
};

#endif
