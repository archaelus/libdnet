/*
 * ip.c
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: ip.c,v 1.2 2001/10/15 01:53:46 dugsong Exp $
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"

struct ip_handle {
	int	fd;
#ifdef HAVE_COOKED_RAWIP
	eth_t		*eth;
	intf_t		*intf;
	arp_t		*arp;
	route_t		*rt;
	struct addr	 ip_src, ip_dst;
	struct addr	 eth_src, eth_dst;
#endif
};

#ifdef HAVE_COOKED_RAWIP
ip_t *
ip_open(void)
{
	ip_t *i;

	if ((i = calloc(1, sizeof(*i))) == NULL)
		return (NULL);
	
	if ((i->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		free(i);
		return (NULL);
	}
	i->ip_src.addr_type = i->ip_dst.addr_type = ADDR_TYPE_IP;
	i->ip_src.addr_bits = i->ip_dst.addr_bits = IP_ADDR_BITS;
	
	i->eth_src.addr_type = i->eth_dst.addr_type = ADDR_TYPE_ETH;
	i->eth_src.addr_bits = i->eth_dst.addr_bits = ETH_ADDR_BITS;
	
	if ((i->intf = intf_open()) == NULL ||
	    (i->arp = arp_open()) == NULL ||
	    (i->rt = route_open()) == NULL) {
		ip_close(i);
		free(i);
		return (NULL);
	}
	return (i);
}
#else /* !HAVE_COOKED_RAWIP */
ip_t *
ip_open(void)
{
	ip_t *i;
	int n, fd, len;

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return (NULL);

	n = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &n, sizeof(n)) < 0) {
		close(fd);
		return (NULL);
	}
#ifdef SO_SNDBUF
	len = sizeof(n);
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, &len) < 0) {
		close(fd);
		return (NULL);
	}
	for (n += 128; n < 1048576; n += 128) {
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, len) < 0) {
			if (errno == ENOBUFS)
				break;
			close(fd);
			return (NULL);
		}
	}
#endif
#ifdef SO_BROADCAST
	n = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &n, sizeof(n)) < 0) {
		close(fd);
		return (NULL);
	}
#endif
	if ((i = malloc(sizeof(*i))) == NULL) {
		close(fd);
		return (NULL);
	}
	i->fd = fd;
	return (i);
}
#endif /* !HAVE_COOKED_RAWIP */

#ifdef HAVE_COOKED_RAWIP
static int
ip_match_intf(char *device, struct addr *addr, int flags, void *arg)
{
	ip_t *i = (ip_t *)arg;
	
	if (addr->addr_ip == i->ip_src.addr_ip ||
	    i->ip_src.addr_ip == IP_ADDR_ANY) {
		if (intf_get(i->intf, device, &i->eth_src, NULL) == 0) {
			if (i->eth != NULL)
				eth_close(i->eth);
			if ((i->eth = eth_open(device)) == NULL)
				return (-1);
			return (1);
		}
	}
	return (0);
}

static int
ip_lookup(ip_t *i, ip_addr_t dst)
{
	struct sockaddr_in sin;
	struct addr gw;
	int n;
	
	i->ip_dst.addr_ip = dst;

	addr_ntos(&i->ip_dst, (struct sockaddr *)&sin);
	sin.sin_port = htons(666);

	/* XXX - Force the kernel to ARP for our destination. */
	if (connect(i->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		return (-1);	/* XXX */

	/* Lookup our source address. */
	n = sizeof(sin);
	if (getsockname(i->fd, (struct sockaddr *)&sin, &n) < 0)
		return (-1);	/* XXX */

	addr_ston((struct sockaddr *)&sin, &i->ip_src);
	
	if (intf_loop(i->intf, ip_match_intf, i) != 1) {
		i->ip_src.addr_ip = IP_ADDR_ANY;
		if (intf_loop(i->intf, ip_match_intf, i) != 1)
			return (-1);
	}
	/* Lookup our destination address. */
	if (arp_get(i->arp, &i->ip_dst, &i->eth_dst) == 0)
		return (0);
	
	if (route_get(i->rt, &i->ip_dst, &gw) == 0) {
		if (gw.addr_ip != i->ip_src.addr_ip &&
		    arp_get(i->arp, &gw, &i->eth_dst) == 0)
			return (0);
	}
	memcpy(&i->eth_dst.addr_eth, ETH_ADDR_BROADCAST, ETH_ADDR_LEN);
	
	return (0);
}

ssize_t
ip_send(ip_t *i, const void *buf, size_t len)
{
	struct ip_hdr *ip;
	struct eth_hdr *eth;
	u_char frame[ETH_LEN_MAX];
	
	ip = (struct ip_hdr *)buf;
	
	if (ip->ip_dst != i->ip_dst.addr_ip) {
		if (ip_lookup(i, ip->ip_dst) < 0)
			return (-1);
	}
	/* XXX - handle fragmentation here */

	eth = (struct eth_hdr *)frame;
	memcpy(&eth->eth_src, &i->eth_src.addr_eth, ETH_ADDR_LEN);
	memcpy(&eth->eth_dst, &i->eth_dst.addr_eth, ETH_ADDR_LEN);
	eth->eth_type = htons(ETH_TYPE_IP);
	
	memcpy(frame + ETH_HDR_LEN, buf, len);
	
	if (eth_send(i->eth, frame, ETH_HDR_LEN + len) != ETH_HDR_LEN + len)
		return (-1);

	return (len);
}
#else /* !HAVE_COOKED_RAWIP */
ssize_t
ip_send(ip_t *i, const void *buf, size_t len)
{
	struct ip_hdr *ip;
	struct sockaddr_in sin;

	ip = (struct ip_hdr *)buf;

	memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SOCKADDR_SA_LEN       
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = ip->ip_dst;
	
#if (defined(BSD) && !defined(OpenBSD)) || defined(UNIXWARE)
	ip->ip_len = ntohs(ip->ip_len);
	ip->ip_off = ntohs(ip->ip_off);

	len = sendto(i->fd, buf, len, 0,
	    (struct sockaddr *)&sin, sizeof(sin));
	
	ip->ip_len = htons(ip->ip_len);
	ip->ip_off = htons(ip->ip_off);

	return (len);
#else
	return (sendto(i->fd, buf, len, 0,
	    (struct sockaddr *)&sin, sizeof(sin)));
#endif
}
#endif /* !HAVE_COOKED_RAWIP */

int
ip_close(ip_t *i)
{
	if (i == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (close(i->fd) < 0)
		return (-1);
#ifdef HAVE_COOKED_RAWIP	
	if (i->intf != NULL)
		intf_close(i->intf);

	if (i->arp != NULL)
		arp_close(i->arp);

	if (i->rt != NULL)
		route_close(i->rt);
#endif
	free(i);
	return (0);
}

void
ip_cksum(struct ip_hdr *ip)
{
	int hl, len, sum;

	hl = ip->ip_hl << 2;
	len = ntohs(ip->ip_len) - hl;
	
	switch (ip->ip_p) {
	case IP_PROTO_TCP:
	{
		struct tcp_hdr *tcp = (struct tcp_hdr *)((u_char *)ip + hl);
		
		tcp->th_sum = 0;
		sum = ip_cksum_add(tcp, len, 0) + htons(ip->ip_p + len);
		sum = ip_cksum_add(&ip->ip_src, 8, sum);
		tcp->th_sum = ip_cksum_carry(sum);
		break;
	}
	case IP_PROTO_UDP:
	{
		struct udp_hdr *udp = (struct udp_hdr *)((u_char *)ip + hl);
		
		udp->uh_sum = 0;
		sum = ip_cksum_add(udp, len, 0) + htons(ip->ip_p + len);
		sum = ip_cksum_add(&ip->ip_src, 8, sum);
		udp->uh_sum = ip_cksum_carry(sum);
		break;
	}
	case IP_PROTO_ICMP:
	case IP_PROTO_IGMP:
	{
		struct icmp_hdr *icmp = (struct icmp_hdr *)((u_char *)ip + hl);
		
		icmp->icmp_cksum = 0;
		sum = ip_cksum_add(icmp, len, 0);
		icmp->icmp_cksum = ip_cksum_carry(sum);
		break;
	}
	default:
		break;
	}
	ip->ip_sum = 0;
	sum = ip_cksum_add(ip, hl, 0);
	ip->ip_sum = ip_cksum_carry(sum);
}

int
ip_cksum_add(void *buf, u_int len, int cksum)
{
	u_short *sp = (u_short *)buf;
	int n, sn;
	
	sn = len / 2;
	n = (sn + 15) / 16;

	/* XXX - unroll loop using Duff's device. */
	switch (sn % 16) {
	case 0:	do {
		cksum += *sp++;
	case 15:
		cksum += *sp++;
	case 14:
		cksum += *sp++;
	case 13:
		cksum += *sp++;
	case 12:
		cksum += *sp++;
	case 11:
		cksum += *sp++;
	case 10:
		cksum += *sp++;
	case 9:
		cksum += *sp++;
	case 8:
		cksum += *sp++;
	case 7:
		cksum += *sp++;
	case 6:
		cksum += *sp++;
	case 5:
		cksum += *sp++;
	case 4:
		cksum += *sp++;
	case 3:
		cksum += *sp++;
	case 2:
		cksum += *sp++;
	case 1:
		cksum += *sp++;
		} while (--n > 0);
	}
	if (len & 1)
		cksum += htons(*(u_char *)sp << 8);

	return (cksum);
}
