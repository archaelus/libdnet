/*
 * arp-none.c
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: arp-none.c,v 1.3 2002/01/09 03:55:46 dugsong Exp $
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
	errno = ENOSYS;
	return (NULL);
}

int
arp_add(arp_t *a, const struct addr *pa, const struct addr *ha)
{
	errno = ENOSYS;
	return (-1);
}

int
arp_delete(arp_t *a, const struct addr *pa)
{
	errno = ENOSYS;
	return (-1);
}

int
arp_get(arp_t *a, const struct addr *pa, struct addr *ha)
{
	errno = ENOSYS;
	return (-1);
}

int
arp_loop(arp_t *a, arp_handler callback, void *arg)
{
	errno = ENOSYS;
	return (-1);
}

int
arp_close(arp_t *a)
{
	errno = ENOSYS;
	return (-1);
}
