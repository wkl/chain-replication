bin_PROGRAMS = server
server_SOURCES = \
	server.cc
nodist_server_SOURCES = \
	$(top_srcdir)/common/message.pb.cc

server_CPPFLAGS = \
	${my_CPPFLAGS} \
	$(BOOST_CPPFLAGS) \
	-I$(top_srcdir)/common/
server_CXXFLAGS = ${my_CXXFLAGS}
server_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIBS) \
	$(BOOST_SYSTEM_LIBS) \
	$(BOOST_ASIO_LIBS) \
	$(PROTOBUF_LIBS)
