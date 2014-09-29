#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([chain-replication], [0.1], [{kelong.wang,dandan.zheng}@stonybrook.edu])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([server/server.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX([clang++ g++])
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

# Checks for libraries.
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
BOOST_REQUIRE([1.54])
BOOST_PROGRAM_OPTIONS # will slow ./configure

# Global flags
my_CXXFLAGS="-g"
my_CPPFLAGS="-Wall -Werror"
AC_SUBST([my_CXXFLAGS])
AC_SUBST([my_CPPFLAGS])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 server/Makefile])
AC_OUTPUT
