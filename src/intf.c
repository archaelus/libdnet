/*
 * intf.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: intf.c,v 1.37 2002/02/26 17:02:21 dugsong Exp $
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <net/if.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"

/* XXX - Tru64 */
#if defined(SIOCRIPMTU) && defined(SIOCSIPMTU)
# define SIOCGIFMTU	SIOCRIPMTU
# define SIOCSIFMTU	SIOCSIPMTU
#endif

/* XXX - HP-UX */
#if defined(SIOCADDIFADDR) && defined(SIOCDELIFADDR)
# define SIOCAIFADDR	SIOCADDIFADDR
# define SIOCDIFADDR	SIOCDELIFADDR
#endif

/* XXX - HP-UX, Solaris */
#if !defined(ifr_mtu) && defined(ifr_metric)
# define ifr_mtu	ifr_metric
#endif

#ifdef HAVE_SOCKADDR_SA_LEN
# define NEXTIFR(i)	((struct ifreq *)((u_char *)&i->ifr_addr + \
				i->ifr_addr.sa_len))
#else
# define NEXTIFR(i)	(i + 1)
#endif

/* XXX - superset of ifreq, for portable SIOC{A,D}IFADDR */
struct dnet_ifaliasreq {
	char		ifra_name[IFNAMSIZ];
	struct sockaddr ifra_addr;
	struct sockaddr ifra_brdaddr;
	struct sockaddr ifra_mask;
	int		ifra_cookie;	/* XXX - IRIX!@#$ */
};

struct intf_handle {
	int		fd;
	struct ifconf	ifc;
	u_char		ifcbuf[4192];
};

static int
intf_flags_to_iff(u_short flags, int iff)
{
	if (flags & INTF_FLAG_UP)
		iff |= IFF_UP;
	else
		iff &= ~IFF_UP;
	if (flags & INTF_FLAG_NOARP)
		iff |= IFF_NOARP;
	else
		iff &= ~IFF_NOARP;
	
	return (iff);
}

static u_int
intf_iff_to_flags(int iff)
{
	u_int n = 0;

	if (iff & IFF_UP)
		n |= INTF_FLAG_UP;	
	if (iff & IFF_LOOPBACK)
		n |= INTF_FLAG_LOOPBACK;
	if (iff & IFF_POINTOPOINT)
		n |= INTF_FLAG_POINTOPOINT;
	if (iff & IFF_NOARP)
		n |= INTF_FLAG_NOARP;
	if (iff & IFF_BROADCAST)
		n |= INTF_FLAG_BROADCAST;
	if (iff & IFF_MULTICAST)
		n |= INTF_FLAG_MULTICAST;

	return (n);
}

intf_t *
intf_open(void)
{
	intf_t *intf;
	
	if ((intf = calloc(1, sizeof(*intf))) == NULL)
		return (NULL);

	if ((intf->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return (intf_close(intf));

	return (intf);
}

static int
_intf_delete_aliases(intf_t *intf, struct intf_entry *entry)
{
	int i;
#if defined(SIOCDIFADDR) && !defined(__linux__)	/* XXX - see Linux below */
	struct dnet_ifaliasreq ifra;
	
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, entry->intf_name, sizeof(ifra.ifra_name));
	
	for (i = 0; i < entry->intf_alias_num; i++) {
		addr_ntos(&entry->intf_alias_addrs[i], &ifra.ifra_addr);
		ioctl(intf->fd, SIOCDIFADDR, &ifra);
	}
#else
	struct ifreq ifr;
	
	for (i = 0; i < entry->intf_alias_num; i++) {
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s:%d",
		    entry->intf_name, i + 1);
# ifdef SIOCLIFREMOVEIF
		/* XXX - overloading Solaris lifreq with ifreq */
		ioctl(intf->fd, SIOCLIFREMOVEIF, &ifr);
# else
		/* XXX - only need to set interface down on Linux */
		ifr.ifr_flags = 0;
		ioctl(intf->fd, SIOCSIFFLAGS, &ifr);
# endif
	}
#endif
	return (0);
}

static int
_intf_add_aliases(intf_t *intf, const struct intf_entry *entry)
{
	int i;
#ifdef SIOCAIFADDR
	struct dnet_ifaliasreq ifra;
	struct addr bcast;
	
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, entry->intf_name, sizeof(ifra.ifra_name));
	
	for (i = 0; i < entry->intf_alias_num; i++) {
		if (addr_ntos(&entry->intf_alias_addrs[i],
		    &ifra.ifra_addr) < 0)
			return (-1);
		addr_bcast(&entry->intf_alias_addrs[i], &bcast);
		addr_ntos(&bcast, &ifra.ifra_brdaddr);
		addr_btos(entry->intf_alias_addrs[i].addr_bits,
		    &ifra.ifra_mask);
		
		if (ioctl(intf->fd, SIOCAIFADDR, &ifra) < 0)
			return (-1);
	}
#else
	struct ifreq ifr;
	
	for (i = 0; i < entry->intf_alias_num; i++) {
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s:%d",
		    entry->intf_name, i + 1);
# ifdef SIOCLIFADDIF
		if (ioctl(intf->fd, SIOCLIFADDIF, &ifr) < 0)
			return (-1);
# endif
		if (addr_ntos(&entry->intf_alias_addrs[i], &ifr.ifr_addr) < 0)
			return (-1);
		if (ioctl(intf->fd, SIOCSIFADDR, &ifr) < 0)
			return (-1);
	}
	strlcpy(ifr.ifr_name, entry->intf_name, sizeof(ifr.ifr_name));
#endif
	return (0);
}

int
intf_set(intf_t *intf, const struct intf_entry *entry)
{
	struct ifreq ifr;
	struct intf_entry *orig;
	struct addr bcast;
	u_char buf[BUFSIZ];
	
	orig = (struct intf_entry *)buf;
	orig->intf_len = sizeof(buf);
	strcpy(orig->intf_name, entry->intf_name);
	
	if (intf_get(intf, orig) < 0)
		return (-1);

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, entry->intf_name, sizeof(ifr.ifr_name));
	
	/* Set interface MTU. */
	if (entry->intf_mtu != 0) {
		ifr.ifr_mtu = entry->intf_mtu;
		if (ioctl(intf->fd, SIOCSIFMTU, &ifr) < 0)
			return (-1);
	}
	/* Set interface address. */
	if (entry->intf_addr.addr_type == ADDR_TYPE_IP) {
#ifdef BSD
		/* XXX - why must this happen before SIOCSIFADDR? */
		if (addr_btos(entry->intf_addr.addr_bits,
		    &ifr.ifr_addr) == 0) {
			if (ioctl(intf->fd, SIOCSIFNETMASK, &ifr) < 0)
				return (-1);
		}
#endif
		if (addr_ntos(&entry->intf_addr, &ifr.ifr_addr) < 0)
			return (-1);
		if (ioctl(intf->fd, SIOCSIFADDR, &ifr) < 0 && errno != EEXIST)
			return (-1);
		
		if (addr_btos(entry->intf_addr.addr_bits,
		    &ifr.ifr_addr) == 0) {
			if (ioctl(intf->fd, SIOCSIFNETMASK, &ifr) < 0)
				return (-1);
		}
		if (addr_bcast(&entry->intf_addr, &bcast) == 0) {
			if (addr_ntos(&bcast, &ifr.ifr_broadaddr) == 0) {
				/* XXX - ignore error from non-broadcast ifs */
				ioctl(intf->fd, SIOCSIFBRDADDR, &ifr);
			}
		}
	}
	/* Set link-level address. */
	if (entry->intf_link_addr.addr_type == ADDR_TYPE_ETH &&
	    addr_cmp(&entry->intf_link_addr, &orig->intf_link_addr) != 0) {
#if defined(SIOCSIFHWADDR)
		if (addr_ntos(&entry->intf_link_addr, &ifr.ifr_hwaddr) < 0)
			return (-1);
		if (ioctl(intf->fd, SIOCSIFHWADDR, &ifr) < 0)
			return (-1);
#elif defined (SIOCSIFLLADDR)
		memcpy(ifr.ifr_addr.sa_data, &entry->intf_link_addr.addr_eth,
		    ETH_ADDR_LEN);
		ifr.ifr_addr.sa_len = ETH_ADDR_LEN;
		if (ioctl(intf->fd, SIOCSIFLLADDR, &ifr) < 0)
			return (-1);
#else
		eth_t *eth;

		if ((eth = eth_open(entry->intf_name)) == NULL)
			return (-1);
		if (eth_set(eth, &entry->intf_link_addr.addr_eth) < 0) {
			eth_close(eth);
			return (-1);
		}
		eth_close(eth);
#endif
	}
	/* Set point-to-point destination. */
	if (entry->intf_dst_addr.addr_type == ADDR_TYPE_IP) {
		if (addr_ntos(&entry->intf_dst_addr, &ifr.ifr_dstaddr) < 0)
			return (-1);
		if (ioctl(intf->fd, SIOCSIFDSTADDR, &ifr) < 0 &&
		    errno != EEXIST)
			return (-1);
	}
	/* Delete any existing aliases. */
	if (_intf_delete_aliases(intf, orig) < 0)
		return (-1);
	
	/* Add aliases. */
	if (_intf_add_aliases(intf, entry) < 0)
		return (-1);
	
	/* Set interface flags. */
	if (ioctl(intf->fd, SIOCGIFFLAGS, &ifr) < 0)
		return (-1);
	
	ifr.ifr_flags = intf_flags_to_iff(entry->intf_flags, ifr.ifr_flags);
	
	if (ioctl(intf->fd, SIOCSIFFLAGS, &ifr) < 0)
		return (-1);
	
	return (0);
}

/* XXX - this is total crap. how to do this without walking ifnet? */
static void
_intf_set_type(struct intf_entry *entry)
{
	if ((entry->intf_flags & INTF_FLAG_BROADCAST) != 0)
		entry->intf_type = INTF_TYPE_ETH;
	else if ((entry->intf_flags & INTF_FLAG_POINTOPOINT) != 0)
		entry->intf_type = INTF_TYPE_TUN;
	else if ((entry->intf_flags & INTF_FLAG_LOOPBACK) != 0)
		entry->intf_type = INTF_TYPE_LOOPBACK;
	else
		entry->intf_type = INTF_TYPE_OTHER;
}

static int
_intf_get_noalias(intf_t *intf, struct intf_entry *entry)
{
	struct ifreq ifr;

	strlcpy(ifr.ifr_name, entry->intf_name, sizeof(ifr.ifr_name));
	
	/* Get interface flags. */
	if (ioctl(intf->fd, SIOCGIFFLAGS, &ifr) < 0)
		return (-1);
	
	entry->intf_flags = intf_iff_to_flags(ifr.ifr_flags);
	_intf_set_type(entry);
	
	/* Get interface MTU. */
	if (ioctl(intf->fd, SIOCGIFMTU, &ifr) < 0)
		return (-1);
	entry->intf_mtu = ifr.ifr_mtu;

	entry->intf_addr.addr_type = entry->intf_dst_addr.addr_type =
	    entry->intf_link_addr.addr_type = ADDR_TYPE_NONE;
	
	/* Get primary interface address. */
	if (ioctl(intf->fd, SIOCGIFADDR, &ifr) == 0) {
		addr_ston(&ifr.ifr_addr, &entry->intf_addr);
		if (ioctl(intf->fd, SIOCGIFNETMASK, &ifr) < 0)
			return (-1);
		addr_stob(&ifr.ifr_addr, &entry->intf_addr.addr_bits);
	}
	/* Get other addresses. */
	if (entry->intf_type == INTF_TYPE_TUN) {
		if (ioctl(intf->fd, SIOCGIFDSTADDR, &ifr) == 0) {
			if (addr_ston(&ifr.ifr_addr,
			    &entry->intf_dst_addr) < 0)
				return (-1);
		}
	} else if (entry->intf_type == INTF_TYPE_ETH) {
#if defined(SIOCGIFHWADDR)
		if (ioctl(intf->fd, SIOCGIFHWADDR, &ifr) < 0)
			return (-1);
		if (addr_ston(&ifr.ifr_addr, &entry->intf_link_addr) < 0)
			return (-1);
#else
		eth_t *eth;
		
		if ((eth = eth_open(entry->intf_name)) != NULL) {
			if (!eth_get(eth, &entry->intf_link_addr.addr_eth)) {
				entry->intf_link_addr.addr_type =
				    ADDR_TYPE_ETH;
				entry->intf_link_addr.addr_bits =
				    ETH_ADDR_BITS;
			}
			eth_close(eth);
		}
#endif
	}
	return (0);
}

#ifdef SIOCLIFADDR
/* XXX - aliases on IRIX don't show up in SIOCGIFCONF */
static int
_intf_get_aliases(intf_t *intf, struct intf_entry *entry)
{
	struct dnet_ifaliasreq ifra;
	struct addr *ap, *lap;
	
	strlcpy(ifra.ifra_name, entry->intf_name, sizeof(ifra.ifra_name));
	addr_ntos(&entry->intf_addr, &ifra.ifra_addr);
	addr_btos(entry->intf_addr.addr_bits, &ifra.ifra_mask);
	memset(&ifra.ifra_brdaddr, 0, sizeof(ifra.ifra_brdaddr));
	ifra.ifra_cookie = 1;

	ap = entry->intf_alias_addrs;
	lap = (struct addr *)((u_char *)entry + entry->intf_len);
	
	while (ioctl(intf->fd, SIOCLIFADDR, &ifra) == 0 &&
	    ifra.ifra_cookie > 0 && ap < lap) {
		if (addr_ston(&ifra.ifra_addr, ap) < 0)
			break;
		ap++, entry->intf_alias_num++;
	}
	entry->intf_len = (u_char *)ap - (u_char *)entry;
	
	return (0);
}
#else
static int
_intf_get_aliases(intf_t *intf, struct intf_entry *entry)
{
	struct ifreq *ifr, *lifr;
	struct addr *ap, *lap;
	char *p;
	
	if (intf->ifc.ifc_len < sizeof(*ifr)) {
		errno = EINVAL;
		return (-1);
	}
	entry->intf_alias_num = 0;
	ap = entry->intf_alias_addrs;
	lifr = (struct ifreq *)&intf->ifc.ifc_buf[intf->ifc.ifc_len];
	lap = (struct addr *)((u_char *)entry + entry->intf_len);
	
	/* Get addresses for this interface. */
	for (ifr = intf->ifc.ifc_req; ifr < lifr && ap < lap;
	    ifr = NEXTIFR(ifr)) {
		/* XXX - Linux, Solaris ifaliases */
		if ((p = strchr(ifr->ifr_name, ':')) != NULL)
			*p = '\0';
		
		if (strcmp(ifr->ifr_name, entry->intf_name) != 0)
			continue;
		
		if (addr_ston(&ifr->ifr_addr, ap) < 0 ||
		    ap->addr_type != ADDR_TYPE_IP)
			continue;
		
		if (ap->addr_ip == entry->intf_addr.addr_ip ||
		    ap->addr_ip == entry->intf_dst_addr.addr_ip)
			continue;
		
		ap++, entry->intf_alias_num++;
	}
	entry->intf_len = (u_char *)ap - (u_char *)entry;
	
	return (0);
}
#endif /* SIOCLIFADDR */

int
intf_get(intf_t *intf, struct intf_entry *entry)
{
	if (_intf_get_noalias(intf, entry) < 0)
		return (-1);
#ifndef SIOCLIFADDR
	intf->ifc.ifc_buf = (caddr_t)intf->ifcbuf;
	intf->ifc.ifc_len = sizeof(intf->ifcbuf);
	
	if (ioctl(intf->fd, SIOCGIFCONF, &intf->ifc) < 0)
		return (-1);
#endif
	return (_intf_get_aliases(intf, entry));
}

#ifdef HAVE_LINUX_PROCFS
#define PROC_DEV_FILE	"/proc/net/dev"

int
intf_loop(intf_t *intf, intf_handler callback, void *arg)
{
	FILE *fp;
	struct intf_entry *entry;
	char *p, buf[BUFSIZ], ebuf[BUFSIZ];
	int ret;

	entry = (struct intf_entry *)ebuf;
	
	if ((fp = fopen(PROC_DEV_FILE, "r")) == NULL)
		return (-1);
	
	intf->ifc.ifc_buf = (caddr_t)intf->ifcbuf;
	intf->ifc.ifc_len = sizeof(intf->ifcbuf);
	
	if (ioctl(intf->fd, SIOCGIFCONF, &intf->ifc) < 0)
		return (-1);

	ret = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if ((p = strchr(buf, ':')) == NULL)
			continue;
		*p = '\0';
		for (p = buf; *p == ' '; p++)
			;
		
		strlcpy(entry->intf_name, p, sizeof(entry->intf_name));
		entry->intf_len = sizeof(ebuf);
		
		if (_intf_get_noalias(intf, entry) < 0) {
			ret = -1;
			break;
		}
		if (_intf_get_aliases(intf, entry) < 0) {
			ret = -1;
			break;
		}
		if ((ret = (*callback)(entry, arg)) != 0)
			break;
	}
	if (ferror(fp))
		ret = -1;
	
	fclose(fp);
	
	return (ret);
}
#else
int
intf_loop(intf_t *intf, intf_handler callback, void *arg)
{
	struct intf_entry *entry;
	struct ifreq *ifr, *lifr, *pifr;
	char *p, ebuf[BUFSIZ];
	int ret;

	entry = (struct intf_entry *)ebuf;

	intf->ifc.ifc_buf = (caddr_t)intf->ifcbuf;
	intf->ifc.ifc_len = sizeof(intf->ifcbuf);
	
	if (ioctl(intf->fd, SIOCGIFCONF, &intf->ifc) < 0)
		return (-1);

	pifr = NULL;
	lifr = (struct ifreq *)&intf->ifc.ifc_buf[intf->ifc.ifc_len];
	
	for (ifr = intf->ifc.ifc_req; ifr < lifr; ifr = NEXTIFR(ifr)) {
		/* XXX - Linux, Solaris ifaliases */
		if ((p = strchr(ifr->ifr_name, ':')) != NULL)
			*p = '\0';
		
		if (pifr != NULL && strcmp(ifr->ifr_name, pifr->ifr_name) == 0)
			continue;

		strlcpy(entry->intf_name, ifr->ifr_name,
		    sizeof(entry->intf_name));
		entry->intf_len = sizeof(ebuf);
		
		if (_intf_get_noalias(intf, entry) < 0)
			return (-1);
		if (_intf_get_aliases(intf, entry) < 0)
			return (-1);
		
		if ((ret = (*callback)(entry, arg)) != 0)
			return (ret);

		pifr = ifr;
	}
	return (0);
}
#endif /* !HAVE_LINUX_PROCFS */

intf_t *
intf_close(intf_t *intf)
{
	if (intf->fd > 0)
		close(intf->fd);
	free(intf);
	return (NULL);
}
