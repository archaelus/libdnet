/*
 * udp.h
 *
 * User Datagram Protocol (RFC 768).
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: udp.h,v 1.5 2001/12/14 14:27:29 dugsong Exp $
 */

#ifndef DNET_UDP_H
#define DNET_UDP_H

#ifdef __cplusplus
extern "C" {
#endif
	
#define UDP_HDR_LEN	8

struct udp_hdr {
	u_short		uh_sport;	/* source port */
	u_short		uh_dport;	/* destination port */
	u_short		uh_ulen;	/* udp length (including header) */
	u_short		uh_sum;		/* udp checksum */
};

#define UDP_PORT_MAX	65535

#define udp_fill_hdr(hdr, sport, dport, ulen) do {		\
	struct udp_hdr *udp_fill_p = (struct udp_hdr *)(hdr);	\
	udp_fill_p->uh_sport = htons(sport);			\
	udp_fill_p->uh_dport = htons(dport);			\
	udp_fill_p->uh_ulen = htons(ulen);			\
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* DNET_UDP_H */
