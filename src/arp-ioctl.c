/*
 * arp-ioctl.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: arp-ioctl.c,v 1.13 2002/01/09 03:55:46 dugsong Exp $
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_STREAMS_MIB2
#include <sys/sockio.h>
#include <sys/stream.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <inet/common.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#undef IP_ADDR_LEN
#elif defined(HAVE_SYS_MIB_H)
#include <sys/mib.h>
#endif

#include <net/if_arp.h>
#ifdef HAVE_STREAMS_MIB2
#include <netinet/in.h>

#include <stropts.h>
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"

#ifdef HAVE_LINUX_PROCFS
#define PROC_ARP_FILE	"/proc/net/arp"
#endif

struct arp_handle {
	int	 fd;
#ifdef HAVE_ARPREQ_ARP_DEV
	intf_t	*intf;
#endif
};

arp_t *
arp_open(void)
{
	arp_t *a;
	
	if ((a = calloc(1, sizeof(*a))) == NULL)
		return (NULL);

#ifdef HAVE_ARPREQ_ARP_DEV
	if ((a->intf = intf_open()) == NULL) {
		free(a);
		return (NULL);
	}
#endif
#ifdef HAVE_STREAMS_MIB2
	if ((a->fd = open(IP_DEV_NAME, O_RDWR)) < 0) {
		arp_close(a);
		return (NULL);
	}
#elif defined(HAVE_STREAMS_ROUTE)
	if ((a->fd = open("/dev/route", O_WRONLY, 0)) < 0) {
		arp_close(a);
		return (NULL);
	}
#else
	if ((a->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		arp_close(a);
		return (NULL);
	}
#endif
	return (a);
}

#ifdef HAVE_ARPREQ_ARP_DEV
static int
arp_set_dev(char *device, struct intf_info *info, void *arg)
{
	struct arpreq *ar = (struct arpreq *)arg;
	struct addr dst;
	uint32_t mask;
	
	addr_btom(info->intf_addr.addr_bits, &mask, IP_ADDR_LEN);
	addr_ston((struct sockaddr *)&ar->arp_pa, &dst);
	
	if ((info->intf_addr.addr_ip & mask) == (dst.addr_ip & mask)) {
		strlcpy(ar->arp_dev, device, sizeof(ar->arp_dev));
		return (1);
	}
	return (0);
}
#endif

int
arp_add(arp_t *a, const struct addr *pa, const struct addr *ha)
{
	struct arpreq ar;

	memset(&ar, 0, sizeof(ar));

	if (addr_ntos(pa, &ar.arp_pa) < 0 ||
	    addr_ntos(ha, &ar.arp_ha) < 0)
		return (-1);

	/* XXX - see arp(7) for details... */
#ifdef __linux__
	ar.arp_ha.sa_family = ARP_HRD_ETH;
#else
	/* XXX - Solaris, HP-UX, others? */
	ar.arp_ha.sa_family = AF_UNSPEC;
#endif

#ifdef HAVE_ARPREQ_ARP_DEV
	if (intf_loop(a->intf, arp_set_dev, &ar) != 1) {
		errno = ESRCH;
		return (-1);
	}
#endif
	ar.arp_flags = ATF_PERM | ATF_COM;
#ifdef hpux
	/* XXX - screwy extended arpreq struct */
	{
		struct sockaddr_in *sin;
		
		ar.arp_hw_addr_len = ETH_ADDR_LEN;
		sin = (struct sockaddr_in *)&ar.arp_pa_mask;
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = IP_ADDR_BROADCAST;
	}
#endif
	if (ioctl(a->fd, SIOCSARP, &ar) < 0)
		return (-1);

#ifdef HAVE_STREAMS_MIB2
	/* XXX - force entry into ipNetToMediaTable. */
	{
		struct sockaddr_in sin;
		int fd;
		
		addr_ntos(pa, (struct sockaddr *)&sin);
		sin.sin_port = htons(666);
		
		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			return (-1);
		
		if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			close(fd);
			return (-1);
		}
		write(fd, NULL, 0);
		close(fd);
	}
#endif
	return (0);
}

int
arp_delete(arp_t *a, const struct addr *pa)
{
	struct arpreq ar;

	memset(&ar, 0, sizeof(ar));
	
	if (addr_ntos(pa, &ar.arp_pa) < 0)
		return (-1);
	
	if (ioctl(a->fd, SIOCDARP, &ar) < 0)
		return (-1);

	return (0);
}

int
arp_get(arp_t *a, const struct addr *pa, struct addr *ha)
{
	struct arpreq ar;

	memset(&ar, 0, sizeof(ar));
	
	if (addr_ntos(pa, &ar.arp_pa) < 0)
		return (-1);
	
#ifdef HAVE_ARPREQ_ARP_DEV
	if (intf_loop(a->intf, arp_set_dev, &ar) != 1) {
		errno = ESRCH;
		return (-1);
	}
#endif
	if (ioctl(a->fd, SIOCGARP, &ar) < 0)
		return (-1);

	if ((ar.arp_flags & ATF_COM) == 0) {
		errno = ESRCH;
		return (-1);
	}
	return (addr_ston(&ar.arp_ha, ha));
}

#ifdef HAVE_LINUX_PROCFS
int
arp_loop(arp_t *a, arp_handler callback, void *arg)
{
	FILE *fp;
	struct addr pa, ha;
	char buf[BUFSIZ], ipbuf[100], macbuf[100], maskbuf[100], devbuf[100];
	int i, type, flags, ret;

	if ((fp = fopen(PROC_ARP_FILE, "r")) == NULL)
		return (-1);

	ret = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		i = sscanf(buf, "%s 0x%x 0x%x %100s %100s %100s\n",
		    ipbuf, &type, &flags, macbuf, maskbuf, devbuf);
		
		if (i < 4 || (flags & ATF_COM) == 0)
			continue;
		
		if (addr_aton(ipbuf, &pa) == 0 &&
		    addr_aton(macbuf, &ha) == 0) {
			if ((ret = callback(&pa, &ha, arg)) != 0)
				break;
		}
	}
	if (ferror(fp)) {
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	
	return (ret);
}
#elif defined (HAVE_STREAMS_MIB2)
int
arp_loop(arp_t *r, arp_handler callback, void *arg)
{
	struct strbuf msg;
	struct T_optmgmt_req *tor;
	struct T_optmgmt_ack *toa;
	struct T_error_ack *tea;
	struct opthdr *opt;
	mib2_ipNetToMediaEntry_t *arp, *arpend;
	u_char buf[8192];
	int flags, rc, atable, ret;

	tor = (struct T_optmgmt_req *)buf;
	toa = (struct T_optmgmt_ack *)buf;
	tea = (struct T_error_ack *)buf;

	tor->PRIM_type = T_OPTMGMT_REQ;
	tor->OPT_offset = sizeof(*tor);
	tor->OPT_length = sizeof(*opt);
	tor->MGMT_flags = T_CURRENT;
	
	opt = (struct opthdr *)(tor + 1);
	opt->level = MIB2_IP;
	opt->name = opt->len = 0;
	
	msg.maxlen = sizeof(buf);
	msg.len = sizeof(*tor) + sizeof(*opt);
	msg.buf = buf;
	
	if (putmsg(r->fd, &msg, NULL, 0) < 0)
		return (-1);
	
	opt = (struct opthdr *)(toa + 1);
	msg.maxlen = sizeof(buf);
	
	for (;;) {
		flags = 0;
		if ((rc = getmsg(r->fd, &msg, NULL, &flags)) < 0)
			return (-1);

		/* See if we're finished. */
		if (rc == 0 &&
		    msg.len >= sizeof(*toa) &&
		    toa->PRIM_type == T_OPTMGMT_ACK &&
		    toa->MGMT_flags == T_SUCCESS && opt->len == 0)
			break;

		if (msg.len >= sizeof(*tea) && tea->PRIM_type == T_ERROR_ACK)
			return (-1);
		
		if (rc != MOREDATA || msg.len < sizeof(*toa) ||
		    toa->PRIM_type != T_OPTMGMT_ACK ||
		    toa->MGMT_flags != T_SUCCESS)
			return (-1);
		
		atable = (opt->level == MIB2_IP && opt->name == MIB2_IP_22);
		
		msg.maxlen = sizeof(buf) - (sizeof(buf) % sizeof(*arp));
		msg.len = 0;
		flags = 0;
		
		do {
			struct addr pa, ha;
			
			rc = getmsg(r->fd, NULL, &msg, &flags);
			
			if (rc != 0 && rc != MOREDATA)
				return (-1);
			
			if (!atable)
				continue;
			
			arp = (mib2_ipNetToMediaEntry_t *)msg.buf;
			arpend = (mib2_ipNetToMediaEntry_t *)
			    (msg.buf + msg.len);

			pa.addr_type = ADDR_TYPE_IP;
			pa.addr_bits = IP_ADDR_BITS;
			
			ha.addr_type = ADDR_TYPE_ETH;
			ha.addr_bits = ETH_ADDR_BITS;

			for ( ; arp < arpend; arp++) {
				pa.addr_ip = arp->ipNetToMediaNetAddress;
				
				memcpy(&ha.addr_eth,
				    arp->ipNetToMediaPhysAddress.o_bytes,
				    ETH_ADDR_LEN);
				
				if ((ret = callback(&pa, &ha, arg)) != 0)
					return (ret);
			}
		} while (rc == MOREDATA);
	}
	return (0);
}
#elif defined(HAVE_SYS_MIB_H)
#define MAX_ARPENTRIES	512	/* XXX */

int
arp_loop(arp_t *r, arp_handler callback, void *arg)
{
	struct nmparms nm;
	struct addr ha, pa;
	mib_ipNetToMediaEnt arpentries[MAX_ARPENTRIES];
	int fd, i, n, ret;
	
	if ((fd = open_mib("/dev/ip", O_RDWR, 0 /* XXX */, 0)) < 0)
		return (-1);
	
	nm.objid = ID_ipNetToMediaTable;
	nm.buffer = arpentries;
	n = sizeof(arpentries);
	nm.len = &n;
	
	if (get_mib_info(fd, &nm) < 0) {
		close_mib(fd);
		return (-1);
	}
	close_mib(fd);

	pa.addr_type = ADDR_TYPE_IP;
	pa.addr_bits = IP_ADDR_BITS;

	ha.addr_type = ADDR_TYPE_ETH;
	ha.addr_bits = ETH_ADDR_BITS;
	
	n /= sizeof(*arpentries);
	ret = 0;
	
	for (i = 0; i < n; i++) {
		if (arpentries[i].Type != INTM_DYNAMIC ||
		    arpentries[i].PhysAddr.o_length != ETH_ADDR_LEN)
			continue;
		
		pa.addr_ip = arpentries[i].NetAddr;
		memcpy(&ha.addr_eth, arpentries[i].PhysAddr.o_bytes,
		    ETH_ADDR_LEN);
		
		if ((ret = callback(&pa, &ha, arg)) != 0)
			break;
	}
	return (ret);
}
#else
int
arp_loop(arp_t *a, arp_handler callback, void *arg)
{
	errno = ENOSYS;
	return (-1);
}
#endif

int
arp_close(arp_t *a)
{
	assert(a != NULL);

	if (a->fd != 0 && close(a->fd) < 0)
		return (-1);
#ifdef HAVE_ARPREQ_ARP_DEV
	if (intf_close(a->intf) < 0)
		return (-1);
#endif
	return (0);
}
