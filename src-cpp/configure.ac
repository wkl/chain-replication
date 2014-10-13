#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([chain-replication], [0.1], [{kelong.wang,dandan.zheng}@stonybrook.edu])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
#AC_CONFIG_SRCDIR([server/server.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX([clang++ g++])
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

# Checks for libraries.
LT_INIT
AC_CONFIG_MACRO_DIR([m4])

# protobuf
PKG_CHECK_MODULES(PROTOBUF, protobuf >= 2.5.0)
AC_SUBST(PROTOBUF_LIBS)
AC_SUBST(PROTOBUF_CFLAGS)
AC_SUBST(PROTOBUF_VERSION)

AC_CHECK_PROG([PROTOC], [protoc], [protoc])
AS_IF([test "x${PROTOC}" == "x"],
	[AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])])

# jsoncpp
PKG_CHECK_MODULES(JSONCPP, jsoncpp >= 0.5.0)

# glog
PKG_CHECK_MODULES(GLOG, libglog >= 0.3.0)

# boost
BOOST_REQUIRE([1.53])
BOOST_PROGRAM_OPTIONS # will slow ./configure
BOOST_SYSTEM
BOOST_ASIO

# Global flags
my_CXXFLAGS="-g"
# my_CPPFLAGS="-Wall -Werror"
my_CPPFLAGS="-Wall"
AC_SUBST([my_CXXFLAGS])
AC_SUBST([my_CPPFLAGS])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
		 common/Makefile
                 server/Makefile
		 client/Makefile])
AC_OUTPUT
