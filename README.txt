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
	$ brew install glog
Ubuntu:
	$ sudo apt-get install libboost-all-dev
	$ sudo apt-get install protobuf-compiler libprotobuf-dev
	$ sudo apt-get install libgoogle-glog-dev
Fedora:
	$ sudo yum install boost-devel
	$ sudo yum install protobuf-compiler protobuf-devel
	$ sudo yum install glog-devel

2. Build from git repo

	$ autoreconf -if  # generate the configure script and Makefile.in files
	$ ./configure
	$ make

(or Build from distribution)
	$ ./configure
	$ make
