/*
 * route-linux.c
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: route-linux.c,v 1.8 2002/01/09 04:15:41 dugsong Exp $
 */

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <net/route.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"

#define PROC_ROUTE_FILE	"/proc/net/route"

struct route_handle {
	int	 fd;
	int	 nlfd;
};

route_t *
route_open(void)
{
	struct sockaddr_nl snl;
	route_t *r;

	if ((r = calloc(1, sizeof(*r))) == NULL)
		return (NULL);

	if ((r->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		free(r);
		return (NULL);
	}
	if ((r->nlfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		close(r->fd);
		free(r);
		return (NULL);
	}
	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;

	if (bind(r->nlfd, (struct sockaddr *)&snl, sizeof(snl)) < 0) {
		route_close(r);
		return (NULL);
	}
	return (r);
}

int
route_add(route_t *r, const struct addr *dst, const struct addr *gw)
{
	struct rtentry rt;

	assert(r != NULL && dst != NULL && gw != NULL);

	memset(&rt, 0, sizeof(rt));

	if (addr_ntos(dst, &rt.rt_dst) < 0 ||
	    addr_ntos(gw, &rt.rt_gateway) < 0)
		return (-1);

	if (dst->addr_bits < IP_ADDR_BITS) {
		rt.rt_flags = RTF_UP | RTF_GATEWAY;
		if (addr_btos(dst->addr_bits, &rt.rt_genmask) < 0)
			return (-1);
	} else {
		rt.rt_flags = RTF_UP | RTF_HOST | RTF_GATEWAY;
		addr_btos(IP_ADDR_BITS, &rt.rt_genmask);
	}
	return (ioctl(r->fd, SIOCADDRT, &rt));
}

int
route_delete(route_t *r, const struct addr *dst)
{
	struct rtentry rt;

	assert(r != NULL && dst != NULL);

	memset(&rt, 0, sizeof(rt));

	if (addr_ntos(dst, &rt.rt_dst) < 0)
		return (-1);

	if (dst->addr_bits < IP_ADDR_BITS) {
		rt.rt_flags = RTF_UP;
		if (addr_btos(dst->addr_bits, &rt.rt_genmask) < 0)
			return (-1);
	} else {
		rt.rt_flags = RTF_UP | RTF_HOST;
		addr_btos(IP_ADDR_BITS, &rt.rt_genmask);
	}
	return (ioctl(r->fd, SIOCDELRT, &rt));
}

int
route_get(route_t *r, const struct addr *dst, struct addr *gw)
{
	static int seq;
	struct nlmsghdr *nmsg;
	struct rtmsg *rmsg;
	struct rtattr *rta;
	struct sockaddr_nl snl;
	struct iovec iov;
	struct msghdr msg;
	u_char buf[512];
	int i;

	if (dst->addr_type != ADDR_TYPE_IP) {
		errno = EINVAL;
		return (-1);
	}
	memset(buf, 0, sizeof(buf));

	nmsg = (struct nlmsghdr *)buf;
	nmsg->nlmsg_len = NLMSG_LENGTH(sizeof(*nmsg)) +
	    RTA_LENGTH(IP_ADDR_LEN);
	nmsg->nlmsg_flags = NLM_F_REQUEST;
	nmsg->nlmsg_type = RTM_GETROUTE;
	nmsg->nlmsg_seq = ++seq;

	rmsg = (struct rtmsg *)(nmsg + 1);
	rmsg->rtm_family = AF_INET;
	rmsg->rtm_dst_len = dst->addr_bits;
	
	rta = RTM_RTA(rmsg);
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH(IP_ADDR_LEN);

	/* XXX - gross hack for default route */
	if (dst->addr_ip == IP_ADDR_ANY) {
		i = htonl(0x60060606);
		memcpy(RTA_DATA(rta), &i, IP_ADDR_LEN);
	} else
		memcpy(RTA_DATA(rta), &dst->addr_ip, IP_ADDR_LEN);
	
	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;

	iov.iov_base = nmsg;
	iov.iov_len = nmsg->nlmsg_len;
	
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &snl;
	msg.msg_namelen = sizeof(snl);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	if (sendmsg(r->nlfd, &msg, 0) < 0)
		return (-1);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	
	if ((i = recvmsg(r->nlfd, &msg, 0)) <= 0)
		return (-1);

	if (nmsg->nlmsg_len < sizeof(*nmsg) || nmsg->nlmsg_len > i ||
	    nmsg->nlmsg_seq != seq) {
		errno = EINVAL;
		return (-1);
	}
	if (nmsg->nlmsg_type == NLMSG_ERROR)
		return (-1);
	
	i -= NLMSG_LENGTH(sizeof(*nmsg));
	
	while (RTA_OK(rta, i)) {
		if (rta->rta_type == RTA_GATEWAY) {
			gw->addr_type = ADDR_TYPE_IP;
			memcpy(&gw->addr_ip, RTA_DATA(rta), IP_ADDR_LEN);
			gw->addr_bits = IP_ADDR_BITS;
			return (0);
		}
		rta = RTA_NEXT(rta, i);
	}
	errno = ESRCH;
	
	return (-1);
}

int
route_loop(route_t *r, route_handler callback, void *arg)
{
	FILE *fp;
	char buf[BUFSIZ], ifbuf[16];
	int i, iflags, refcnt, use, metric, mss, win, irtt, ret;
	struct addr dst, gw;
	uint32_t mask;

	dst.addr_type = gw.addr_type = ADDR_TYPE_IP;
	dst.addr_bits = gw.addr_bits = IP_ADDR_BITS;

	if ((fp = fopen(PROC_ROUTE_FILE, "r")) == NULL)
		return (-1);

	ret = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		i = sscanf(buf,
		    "%16s %X %X %X %d %d %d %X %d %d %d\n",
		    ifbuf, &dst.addr_ip, &gw.addr_ip, &iflags, &refcnt,
		    &use, &metric, &mask, &mss, &win, &irtt);

		if (i < 10 || !(iflags & RTF_UP))
			continue;
		
		if (gw.addr_ip == IP_ADDR_ANY)
			continue;

		dst.addr_type = gw.addr_type = ADDR_TYPE_IP;
		
		if (addr_mtob(&mask, IP_ADDR_LEN, &dst.addr_bits) < 0)
			continue;
		
		if ((ret = callback(&dst, &gw, arg)) != 0)
			break;
	}
	if (ferror(fp)) {
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	
	return (ret);
}

int
route_close(route_t *r)
{
	assert(r != NULL);

	if (close(r->fd) < 0 || close(r->nlfd) < 0)
		return (-1);
	
	free(r);
	return (0);
}
