INSTRUCTIONS
------------

[Dependency: C++]

- Autotools
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
	$ brew install https://raw.githubusercontent.com/Homebrew/homebrew/9e30a42153889a81a813c8689cfa3f51ec3364b9/Library/Formula/jsoncpp.rb
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
	$ ./run_case.sh 1
	$ ./run_case.sh 2
	$ ./run_case.sh 3
	$ ./run_case.sh 4

3. Logging
	
	$ cd <chain-replication>/logs
	$ cat server_bank1_No1.INFO
	$ cat server_bank2_No1.INFO
	$ cat server_bank2_No2.INFO

[Running: Distalgo]

	$ cd <chain-replication>/src-da
	$ python3 -m da chain.da ../config/test1.json		# assume distalgo(1.0.0b8) is installed
or	$ python3 -m da -f -F info -L info chain.da ../config/test1.json	# also log to file


MAIN FILES
----------

src-cpp/
  server/server.{h,cc}	# Chain server code
  client/client.{h,cc}	# Client code
  common/message.{h,cc,proto}	# Communication
  common/{bank,account,common}.h	# Misc
src-da/
  chain.da	# Distalgo code for Chain server and client


BUGS AND LIMITATIONS
--------------------


CONTRIBUTIONS
-------------
C++
- Communication (Kelong)
- Chain/Bank state machine (Dandan)
- Logging (Dandan)
- Configuration (Dandan)

Distalgo (Python)
- Chain/Bank state machine (Kelong)
- Logging (Kelong)
- Configuration (Dandan)

Common / Misc
- Test case generating (Dandan)
- C++ Automake (Kelong)


OTHER COMMENTS
--------------
- Pseudo-code may be not updated.
