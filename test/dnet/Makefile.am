## $Id: Makefile.am,v 1.8 2002/12/02 04:16:23 dugsong Exp $

include $(top_srcdir)/Makefile.am.common

sbin_PROGRAMS = dnet

dnet_SOURCES = addr.c arp.c aton.c aton.h dnet.c eth.c fw.c hex.c \
	icmp.c intf.c ip.c mod.h rand.c route.c send.c tcp.c udp.c

man_MANS = dnet.8

EXTRA_DIST = $(man_MANS)

LDADD = $(top_builddir)/src/libdnet.a

dnet.8.txt: dnet.8
	groff -t -e -mdoc -Tascii dnet.8 | col -bx > $@

