/*
 * ip-cooked.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: ip-cooked.c,v 1.5 2002/02/04 05:54:51 dugsong Exp $
 */

#include "config.h"

#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"
#include "queue.h"

struct ip_intf {
	eth_t			*eth;
	char			 name[60];
	struct addr		 ha;
	struct addr		 pa;
	int			 mtu;
	LIST_ENTRY(ip_intf)	 next;
};

struct ip_handle {
	arp_t			*arp;
	intf_t			*intf;
	route_t			*route;
	int			 fd;
	struct sockaddr_in	 sin;
	
	LIST_HEAD(, ip_intf)	 ip_intf_list;
};

static int
_add_ip_intf(const struct intf_entry *entry, void *arg)
{
	ip_t *ip = (ip_t *)arg;
	struct ip_intf *ipi;

	if (entry->intf_type == INTF_TYPE_ETH &&
	    (entry->intf_flags & INTF_FLAG_UP) != 0 &&
	    entry->intf_mtu >= ETH_LEN_MIN &&
	    entry->intf_addr != NULL && entry->intf_link_addr != NULL) {
		
		if ((ipi = calloc(1, sizeof(*ipi))) == NULL)
			return (-1);
		
		strlcpy(ipi->name, entry->intf_name, sizeof(ipi->name));
		memcpy(&ipi->ha, entry->intf_link_addr, sizeof(ipi->ha));
		memcpy(&ipi->pa, entry->intf_addr, sizeof(ipi->pa));
		ipi->mtu = entry->intf_mtu;

		LIST_INSERT_HEAD(&ip->ip_intf_list, ipi, next);
	}
	return (0);
}

ip_t *
ip_open(void)
{
	ip_t *ip;

	if ((ip = calloc(1, sizeof(*ip))) == NULL)
		return (NULL);

	if ((ip->arp = arp_open()) == NULL ||
	    (ip->intf = intf_open()) == NULL ||
	    (ip->route = route_open()) == NULL)
		return (ip_close(ip));

	if ((ip->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return (ip_close(ip));

	memset(&ip->sin, 0, sizeof(ip->sin));
	ip->sin.sin_family = AF_INET;
	ip->sin.sin_port = htons(666);
	
	LIST_INIT(&ip->ip_intf_list);

	if (intf_loop(ip->intf, _add_ip_intf, ip) != 0)
		return (ip_close(ip));
	
	return (ip);
}

static struct ip_intf *
_lookup_ip_intf(ip_t *ip, ip_addr_t dst)
{
	struct ip_intf *ipi;
	int n;

	ip->sin.sin_addr.s_addr = dst;
	n = sizeof(ip->sin);
	
	if (connect(ip->fd, (struct sockaddr *)&ip->sin, n) < 0)
		return (NULL);

	if (getsockname(ip->fd, (struct sockaddr *)&ip->sin, &n) < 0)
		return (NULL);

	LIST_FOREACH(ipi, &ip->ip_intf_list, next) {
		if (ipi->pa.addr_ip == ip->sin.sin_addr.s_addr) {
			if (ipi->eth == NULL) {
				if ((ipi->eth = eth_open(ipi->name)) == NULL)
					return (NULL);
			}
			if (ipi != LIST_FIRST(&ip->ip_intf_list)) {
				LIST_REMOVE(ipi, next);
				LIST_INSERT_HEAD(&ip->ip_intf_list, ipi, next);
			}
			return (ipi);
		}
	}
	return (NULL);
}

size_t
ip_send(ip_t *ip, const void *buf, size_t len)
{
	struct ip_hdr *iph;
	struct ip_intf *ipi;
	struct arp_entry arpent;
	struct route_entry rtent;
	u_char frame[ETH_LEN_MAX];

	iph = (struct ip_hdr *)buf;
	
	if ((ipi = _lookup_ip_intf(ip, iph->ip_dst)) == NULL) {
		errno = EHOSTUNREACH;
		return (-1);
	}
	arpent.arp_pa.addr_type = ADDR_TYPE_IP;
	arpent.arp_pa.addr_bits = IP_ADDR_BITS;
	arpent.arp_pa.addr_ip = iph->ip_dst;
	
	if (arp_get(ip->arp, &arpent) != 0) {
		memcpy(&rtent.route_dst, &arpent.arp_pa,
		    sizeof(rtent.route_dst));
		
		if (route_get(ip->route, &rtent) == 0) {
			memcpy(&arpent.arp_pa, &rtent.route_gw,
			    sizeof(arpent.arp_pa));
			
			if (arp_get(ip->arp, &arpent) < 0)
				memset(&arpent.arp_ha.addr_eth, 0xff,
				    ETH_ADDR_LEN);
		} else
			memset(&arpent.arp_ha.addr_eth, 0xff, ETH_ADDR_LEN);
	}
	eth_fill_hdr(frame, arpent.arp_ha.addr_eth,
	    ipi->ha.addr_eth, ETH_TYPE_IP);

	if (len > ipi->mtu) {
		u_char *p, *start, *end, *ip_data;
		int ip_hl, fraglen;
		
		ip_hl = iph->ip_hl << 2;
		fraglen = ipi->mtu - ip_hl;

		iph = (struct ip_hdr *)(frame + ETH_HDR_LEN);
		memcpy(iph, buf, ip_hl);
		ip_data = (u_char *)iph + ip_hl;

		start = (u_char *)buf + ip_hl;
		end = (u_char *)buf + len;
		
		for (p = start; p < end; ) {
			memcpy(ip_data, p, fraglen);
			
			iph->ip_len = htons(ip_hl + fraglen);
			iph->ip_off = htons(((p + fraglen < end) ? IP_MF : 0) |
			    ((p - start) >> 3));

			ip_checksum(iph, ip_hl + fraglen);
			
			if (eth_send(ipi->eth, frame,
			    ETH_HDR_LEN + ip_hl + fraglen) < 0)
				return (-1);
			p += fraglen;
			if (end - p < fraglen)
				fraglen = end - p;
		}
		return (len);
	}
	memcpy(frame + ETH_HDR_LEN, buf, len);
	
	if (eth_send(ipi->eth, frame, ETH_HDR_LEN + len) != ETH_HDR_LEN + len)
		return (-1);
	
	return (len);
}

ip_t *
ip_close(ip_t *ip)
{
	struct ip_intf *ipi, *nxt;
	
	for (ipi = LIST_FIRST(&ip->ip_intf_list);
	    ipi != LIST_END(&ip->ip_intf_list); ipi = nxt) {
		nxt = LIST_NEXT(ipi, next);
		if (ipi->eth != NULL)
			eth_close(ipi->eth);
		free(ipi);
	}
	if (ip->fd > 0)		close(ip->fd);
	if (ip->route != NULL)	route_close(ip->route);
	if (ip->intf != NULL)	intf_close(ip->intf);
	if (ip->arp != NULL)	arp_close(ip->arp);
	free(ip);
	
	return (NULL);
}
