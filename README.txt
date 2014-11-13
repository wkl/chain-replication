INSTRUCTIONS
------------

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

1. Run test cases manually [TODO edit]

	$ cd <chain-replication>/src-cpp
	$ server/server -c ../config/test4.json -b bank1 -n 1 -l ../logs &
	$ server/server -c ../config/test4.json -b bank2 -n 1 -l ../logs &
	$ server/server -c ../config/test4.json -b bank2 -n 2 -l ../logs &
	$ server/server -c ../config/test4.json -b bank3 -n 1 -l ../logs &
	$ server/server -c ../config/test4.json -b bank3 -n 2 -l ../logs &
	$ server/server -c ../config/test4.json -b bank3 -n 3 -l ../logs &
	$ client/client -c ../config/test4.json -l ../logs
	$ killall server

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
  master/master.{h,cc}	# Chain server code
  server/server.{h,cc}	# Chain server code
  client/client.{h,cc}	# Client code
  common/message.{h,cc,proto}	# Communication
  common/{bank,account,common}.h	# Misc
src-da/
  chain.da	# Distalgo code for Chain server and client


BUGS AND LIMITATIONS
--------------------

- The send() in Distalgo always succeeds even though the destination is down.
  This behavior impacts how we implement aborting extending gracefully.

LANGUAGE COMPARISON
-------------------

- Size (including comments, empty lines)
Distalgo: 1000 lines
C++: 3000 lines

- Development
To begin with C++, we have to take care of low-level communication stuffs.
We spend some time to investigate the existing framework and build a simple RPC
based on protobuf and Boost.Asio. For the miscellaneous utilities, we use the
existing libraries as much as we can. With these popular libraries and
Autotools to integrate them, it is efficient to develop once we become familiar
with them after some time and effort.

About Distalgo, there are some examples to start with. After a quick learning,
one can focus on the algorithm implementation without worrying about socket,
data packing or event model. It is quite smooth to develop in this phase.
Of course, the data structure in Python is simpler to manipulate than C++.

- Debug
The debug procedure in both language is similar: run and inspect the log files.
In C++, we use shell scripts to run the processes. We feel it is more convenient
to manage the processes in Distalgo. But the Process name in terms of TCP
address is not that meaningful when the number of processes is large, so we
encapsulate the Process inside a new class 'Node' to make the instances in log
file more readable. Concise and clean logging format help us a lot.

- Other
Python syntax as well as Distalgo syntax we use are definitely more readable
and more similar to pseudo-code than C++ (actually we write our pseudo-code in
Python-like syntax). The message sending and receiving handling in Distalgo are
well placed and intuitive to read but we can not find a place to perform certain
routine in all "receive" functions (say we want to increase message counter in
each receiving).


CONTRIBUTIONS
-------------

C++
- Communication (Kelong)
- Master/Server/Client state machine (Dandan)
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
