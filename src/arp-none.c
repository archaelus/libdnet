/*
 * arp-none.c
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: arp-none.c,v 1.1 2001/10/11 04:14:55 dugsong Exp $
 */

#include "config.h"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "dnet.h"

arp_t *
arp_open(void)
{
	errno = EOPNOTSUPP;
	return (NULL);
}

int
arp_add(arp_t *a, struct addr *pa, struct addr *ha)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
arp_delete(arp_t *a, struct addr *pa)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
arp_get(arp_t *a, struct addr *pa, struct addr *ha)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
arp_loop(arp_t *a, arp_handler callback, void *arg)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
arp_close(arp_t *a)
{
	errno = EOPNOTSUPP;
	return (-1);
}
