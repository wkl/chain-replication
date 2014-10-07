#ifndef MASTER_H
#define MASTER_H

#include <string>

#include "node.h"

using namespace std;

class Master : public Node
{
public:
  //constructor
  Master(){};
  Master(string ip, int port)
	:Node(ip, port){};	
  
private:
};

#endif
