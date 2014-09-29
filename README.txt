INSTRUCTIONS
------------

[Dependency]

- Autotools
- Boost (v1.54.0 is known to work)
- Protocol Buffers (v2.5.0 is known to work)

[Compilation: C++]

1. Install library dependencies

Mac OS X:
	$ brew install boost
	$ brew install protobuf
Ubuntu:
	$ sudo apt-get install libboost-all-dev

2. Build from git repo

	$ autoreconf -if  # generate the configure script and Makefile.in files
	$ ./configure
	$ make

(or Build from distribution)
	$ ./configure
	$ make
