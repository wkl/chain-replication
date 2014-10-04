bin_PROGRAMS = client
client_SOURCES = \
	client.cc
nodist_client_SOURCES = \
	@top_srcdir@/common/message.pb.cc

client_CPPFLAGS = \
	${my_CPPFLAGS} \
	$(BOOST_CPPFLAGS) \
	-I@top_srcdir@/common/
client_CXXFLAGS = ${my_CXXFLAGS}
client_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIBS) \
	$(BOOST_SYSTEM_LIBS) \
	$(BOOST_ASIO_LIBS) \
	$(PROTOBUF_LIBS)
