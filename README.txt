INSTRUCTIONS
------------

[Dependency]

- Autotools
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


2. Run test cases (for example)

server/server -c ../config/test4.json -b bank1 -n 1 -l ../logs
server/server -c ../config/test4.json -b bank2 -n 1 -l ../logs
server/server -c ../config/test4.json -b bank2 -n 2 -l ../logs
server/server -c ../config/test4.json -b bank3 -n 1 -l ../logs
server/server -c ../config/test4.json -b bank3 -n 2 -l ../logs
server/server -c ../config/test4.json -b bank3 -n 3 -l ../logs
client/client -c ../config/test4.json -l ../logs


