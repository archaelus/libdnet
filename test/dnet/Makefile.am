## $Id: Makefile.am,v 1.1 2001/10/11 04:14:46 dugsong Exp $

include $(top_srcdir)/Makefile.am.common

bin_PROGRAMS = dnet

dnet_SOURCES = dnet-int.h dnet.c eth.c arp.c ip.c icmp.c tcp.c udp.c

LDADD = $(top_srcdir)/src/libdnet.a
