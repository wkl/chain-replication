%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(builddir) $^

dist_noinst_DATA = message.proto

BUILT_SOURCES = message.pb.h

MOSTLYCLEANFILES = message.pb.cc message.pb.h
