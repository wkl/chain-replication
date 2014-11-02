bin_PROGRAMS = master
master_SOURCES = \
	master.cc \
	@top_srcdir@/common/message.cc
nodist_master_SOURCES = \
	@top_srcdir@/common/message.pb.cc

master_CPPFLAGS = \
	${my_CPPFLAGS} \
	$(BOOST_CPPFLAGS) \
	$(JSONCPP_CFLAGS) \
	$(GLOG_CFLAGS) \
	-I@top_srcdir@/common/
master_CXXFLAGS = ${my_CXXFLAGS}
master_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIBS) \
	$(BOOST_SYSTEM_LIBS) \
	$(BOOST_ASIO_LIBS) \
	$(JSONCPP_LIBS) \
	$(GLOG_LIBS) \
	$(PROTOBUF_LIBS)
