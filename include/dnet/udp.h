/*
 * udp.h
 *
 * User Datagram Protocol (RFC 768).
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: udp.h,v 1.1 2001/10/11 04:14:49 dugsong Exp $
 */

#ifndef DNET_UDP_H
#define DNET_UDP_H

#define UDP_HDR_LEN	8

struct udp_hdr {
	u_short		uh_sport;	/* source port */
	u_short		uh_dport;	/* destination port */
	u_short		uh_ulen;	/* udp length (including header) */
	u_short		uh_sum;		/* udp checksum */
};

#define UDP_PORT_MAX	65535

#endif /* DNET_UDP_H */
