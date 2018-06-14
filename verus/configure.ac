#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Verus], [1.0], [yasir.zaki@nyu.edu])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE(-Wall -Werror foreign)
AC_CONFIG_SRCDIR([src/verus.hpp])
AC_CONFIG_HEADERS([config.h])

AC_LANG(C++)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CONFIG_MACRO_DIR([m4])
AX_BOOST_BASE([1.54])
AX_BOOST_SYSTEM()
AC_CHECK_LIB([tbb], [main], [], [echo "libtbb not found.  Please install the tbb before proceeding."; exit -1])
AC_CHECK_LIB([pthread], [pthread_mutex_init], [], [echo "pthreads not found.  Please install pthread library before proceeding"; exit -1])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset socket strerror])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
