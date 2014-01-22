#
# Copyright (c) 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_HEADERS = netbench_private.h

noinst_LTLIBRARIES = libnb.la
libnb_la_SOURCES = libnb.c ping.c traceroute.c http.c
libnetbench_la_LDFLAGS = -lresolv

CLEANFILES = *~
