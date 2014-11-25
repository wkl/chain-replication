INSTRUCTIONS
------------

(Phase 4 is implemented with C++)

[Dependency: C++]

- Autotools / pkg-config
- Clang (v3.4.0 is known to work) / GCC (v4.8.2 is known to work)
- Boost (v1.54.0 is known to work)
- Protocol Buffers (v2.5.0 is known to work)
- JsonCpp (v0.5.0 is known to work)
- Google Log (v0.3.3 is known to work)

[Compilation: C++]

1. Install library dependencies

Mac OS X:
	$ brew install boost
	$ brew install protobuf
	$ brew install jsoncpp
	$ brew install glog
Ubuntu:
	$ sudo apt-get install libboost-all-dev
	$ sudo apt-get install protobuf-compiler libprotobuf-dev
	$ sudo apt-get install libjsoncpp-dev
	$ sudo apt-get install libgoogle-glog-dev
Fedora:
	$ sudo yum install boost-devel
	$ sudo yum install protobuf-compiler protobuf-devel
	$ sudo yum install jsoncpp-devel
	$ sudo yum install glog-devel

2. Build from git repo

	$ autoreconf -if  # generate the configure script and Makefile.in files
	$ ./configure
	$ make

(or Build from distribution)
	$ ./configure
	$ make

[Running: C++]

1. Run test cases manually

	$ cd <chain-replication>/src-cpp
	$ master/master -c ../config/test1.json -l ../logs &
	$ server/server -c ../config/test1.json -b bank1 -n 1 -l ../logs &
	$ server/server -c ../config/test1.json -b bank1 -n 2 -l ../logs &
	$ client/client -c ../config/test1.json -l ../logs
	$ killall server
	$ killall master

2. Run test cases with script (preferred)

	$ cd <chain-replication>/src-cpp
	$ ./run_case_server.sh 1
	$ ./run_case_client.sh 1
	$ ./run_case_server.sh 2
	$ ./run_case_client.sh 2
	...

3. Logging

	$ cd <chain-replication>/logs
	$ cat server_bank1_No1.INFO
	$ cat server_bank2_No1.INFO
	$ cat server_bank2_No2.INFO
	...

[Running: Distalgo]

Assume Distalgo(1.0.0b8) is installed

	$ cd <chain-replication>/src-da
	$ python3 -m da chain.da ../config/test1.json
	$ python3 -m da chain.da ../config/test2.json
	...

or append log to 'chain.log':
        $ python3 -m da -f -F info -L info chain.da ../config/test1.json

or append log to specific file:
        $ python3 -m da -f --logfilename test1.log -F info -L info chain.da ../config/test1.json

MAIN FILES
----------

src-cpp/
  master/master.{h,cc}	# Master server code
  server/server.{h,cc}	# Chain server code
  client/client.{h,cc}	# Client code
  common/message.{h,cc,proto}	# Communication
  common/{bank,account,common}.h	# Misc
src-da/
  chain.da	# Distalgo code for Master, Chain server and Client


CONTRIBUTIONS
-------------

C++
- Communication (Kelong)
- Master/Server/Client state machine (Dandan, Kelong)
- Logging (Dandan)
- Configuration (Dandan)

Distalgo (Python)
- Master/Server/Client state machine (Kelong)
- Logging (Kelong)
- Configuration (Dandan)

Common / Misc
- Test case generating (Dandan)
- C++ Automake (Kelong)


OTHER COMMENTS
--------------

- Pseudo-code may be not updated.
