/*
 * arp-win32.c
 *
 * Copyright (c) 2002 Dug Song <dugsong@monkey.org>
 *
 * $Id: arp-win32.c,v 1.7 2002/02/02 04:15:57 dugsong Exp $
 */

#include "config.h"

#include <ws2tcpip.h>
#include <Iphlpapi.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dnet.h"

struct arp_handle {
	int	fd;
};

arp_t *
arp_open(void)
{
	arp_t *arp;

	if ((arp = calloc(1, sizeof(*arp))) == NULL)
		return (NULL);

	return (arp);
}

int
arp_add(arp_t *arp, const struct arp_entry *entry)
{
	MIB_IPFORWARDROW ipfrow;
	MIB_IPNETROW iprow;
	
	if (GetBestRoute(entry->arp_pa.addr_ip,
	    IP_ADDR_ANY, &ipfrow) != NO_ERROR)
		return (-1);

	iprow.dwIndex = ipfrow.dwForwardIfIndex;
	iprow.dwPhysAddrLen = ETH_ADDR_LEN;
	memcpy(iprow.bPhysAddr, &entry->arp_ha.addr_eth, ETH_ADDR_LEN);
	iprow.dwAddr = entry->arp_pa.addr_ip;
	iprow.dwType = 4;	/* XXX - static */

	if (CreateIpNetEntry(&iprow) != NO_ERROR)
		return (-1);

	return (0);
}

int
arp_delete(arp_t *arp, const struct arp_entry *entry)
{
	MIB_IPFORWARDROW ipfrow;
	MIB_IPNETROW iprow;

	if (GetBestRoute(entry->arp_pa.addr_ip,
	    IP_ADDR_ANY, &ipfrow) != NO_ERROR)
		return (-1);

	memset(&iprow, 0, sizeof(iprow));
	iprow.dwIndex = ipfrow.dwForwardIfIndex;
	iprow.dwAddr = entry->arp_pa.addr_ip;

	if (DeleteIpNetEntry(&iprow) != NO_ERROR)
		return (-1);

	return (0);
}

static int
_arp_get_entry(const struct arp_entry *entry, void *arg)
{
	struct arp_entry *e = (struct arp_entry *)arg;
	
	if (addr_cmp(&entry->arp_pa, &e->arp_pa) == 0) {
		memcpy(&e->arp_ha, &entry->arp_ha, sizeof(e->arp_ha));
		return (1);
	}
	return (0);
}

int
arp_get(arp_t *arp, struct arp_entry *entry)
{
	if (arp_loop(arp, _arp_get_entry, entry) != 1) {
		errno = ENXIO;
		SetLastError(ERROR_NO_DATA);
		return (-1);
	}
	return (0);
}

int
arp_loop(arp_t *arp, arp_handler callback, void *arg)
{
	MIB_IPNETTABLE *iptable;
	ULONG len;
	struct arp_entry entry;
	u_char buf[2048];
	int i, ret;
	
	iptable = (MIB_IPNETTABLE *)buf;
	len = sizeof(buf);
	
	if (GetIpNetTable(iptable, &len, FALSE) != NO_ERROR)
		return (-1);

	entry.arp_pa.addr_type = ADDR_TYPE_IP;
	entry.arp_pa.addr_bits = IP_ADDR_BITS;
	
	entry.arp_ha.addr_type = ADDR_TYPE_ETH;
	entry.arp_ha.addr_bits = ETH_ADDR_BITS;
	
	for (i = 0; i < iptable->dwNumEntries; i++) {
		if (iptable->table[i].dwPhysAddrLen != ETH_ADDR_LEN)
			continue;
		entry.arp_pa.addr_ip = iptable->table[i].dwAddr;
		memcpy(&entry.arp_ha.addr_eth, iptable->table[i].bPhysAddr,
		    ETH_ADDR_LEN);
		
		if ((ret = (*callback)(&entry, arg)) != 0)
			return (ret);
	}
	return (0);
}

arp_t *
arp_close(arp_t *arp)
{
	free(arp);
	return (NULL);
}
