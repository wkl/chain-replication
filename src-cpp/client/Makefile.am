bin_PROGRAMS = client
client_SOURCES = \
	client.cc \
	@top_srcdir@/common/message.cc
nodist_client_SOURCES = \
	@top_srcdir@/common/message.pb.cc

client_CPPFLAGS = \
	${my_CPPFLAGS} \
	$(BOOST_CPPFLAGS) \
	$(JSONCPP_CFLAGS) \
	$(GLOG_CFLAGS) \
	-I@top_srcdir@/common/
client_CXXFLAGS = ${my_CXXFLAGS}
client_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIBS) \
	$(BOOST_SYSTEM_LIBS) \
	$(BOOST_ASIO_LIBS) \
	$(JSONCPP_LIBS) \
	$(GLOG_LIBS) \
	$(PROTOBUF_LIBS)
