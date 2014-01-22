#
# Copyright (c) 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

AC_PREREQ(2.59)
AC_INIT([netbench], [0.0.1], [bug@scyphus.co.jp])
AC_CONFIG_SRCDIR([toolset/netbench.c])
AC_CONFIG_HEADER([config.h])

# Get hostname and other information.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AC_SUBST(SUBDIRS)

# arguments
AC_ARG_ENABLE(debug,
  [  --enable-debug    Turn on debugging [default no]],
  [case "${enableval}" in
    yes) debug=yes; AC_MSG_RESULT(Checking for debug... yes); AC_DEFINE(DEBUG, 1, debugging option) ;;
    no)  debug=no;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
  esac],[debug=no])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)

# Checks for hosts
case $host_os in
     freebsd*)
        AC_DEFINE(TARGET_FREEBSD, 1, running on FreeBSD)
        arch=freebsd
        ;;
     netbsd*)
        AC_DEFINE(TARGET_NETBSD, 1, running on NetBSD)
        arch=netbsd
        ;;
     darwin*)
        AC_DEFINE(TARGET_DARWIN, 1, running on Mac OS X)
        arch=darwin
        ;;
     linux*)
        AC_DEFINE(TARGET_LINUX, 1, running on Linux)
        arch=linux
        ;;
     *) ;;
esac

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

# Checks for libraries.

enable_logfile_mask=${enable_logfile_mask:-0644}
AC_DEFINE_UNQUOTED(LOGFILE_MASK, ${enable_logfile_mask}, Mask for log files)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
#AC_CHECK_FUNCS([])

# configure date
CONFDATE=`date '+%Y%m%d'`
AC_SUBST(CONFDATE)

# set paths for state directory
if test "${prefix}" = "NONE"; then
  netbench_statedir_prefix="";
else
  netbench_statedir_prefix=${prefix}
fi
if test "${localstatedir}" = '${prefix}/var'; then
  AC_CACHE_CHECK(state directory,ac_cv_statedir,
  [for NETBENCH_STATE_DIR in ${netbench_statedir_prefix}/var/run dnl
                        ${netbench_statedir_prefix}/var/adm dnl
                        ${netbench_statedir_prefix}/etc dnl
                        /var/run dnl
                        /var/adm dnl
                        /etc dnl
                        /dev/null;
  do
    test -d $NETBENCH_STATE_DIR && break
  done
  netbench_statedir=$NETBENCH_STATE_DIR])
else
  netbench_statedir=${localstatedir}
  AC_MSG_CHECKING(directory to use for state file)
  AC_MSG_RESULT(${netbench_statedir})
  AC_SUBST(netbench_statedir)
fi
if test $netbench_statedir = "/dev/null"; then
    AC_MSG_ERROR('STATE DIRECTORY NOT FOUND! FIX OR SPECIFY --localstatedir!')
fi


AC_CONFIG_FILES([Makefile include/Makefile libnb/Makefile toolset/Makefile])
AC_OUTPUT


echo "
NetBench configuration
------------------------
NetBench version        : ${PACKAGE_VERSION}
host operationg system  : ${host_os}
source code location    : ${srcdir}
compiler                : ${CC}
compiler flags          : ${CFLAGS}
linker flags            : ${LDFLAGS} ${LIBS}
state file directory    : ${netbench_statedir}
log file mask           : ${enable_logfile_mask}
"
