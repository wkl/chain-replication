bin_PROGRAMS = client
client_SOURCES = \
	client.cc \
	@top_srcdir@/common/message.cc 
nodist_client_SOURCES = \
	@top_srcdir@/common/message.pb.cc

client_CPPFLAGS = \
	${my_CPPFLAGS} \
	$(BOOST_CPPFLAGS) \
	-I@top_srcdir@/common/ \
	-I@top_srcdir@/server/
client_CXXFLAGS = ${my_CXXFLAGS}
client_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIBS) \
	$(BOOST_SYSTEM_LIBS) \
	$(BOOST_ASIO_LIBS) \
	$(PROTOBUF_LIBS)
