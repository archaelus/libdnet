/*
 * eth-none.c
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: eth-none.c,v 1.4 2002/01/09 04:01:02 dugsong Exp $
 */

#include "config.h"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "dnet.h"

eth_t *
eth_open(const char *device)
{
	errno = ENOSYS;
	return (NULL);
}

size_t
eth_send(eth_t *e, const void *buf, size_t len)
{
	errno = ENOSYS;
	return (-1);
}

int
eth_close(eth_t *e)
{
	errno = ENOSYS;
	return (-1);
}

int
eth_get(eth_t *e, eth_addr_t *ea)
{
	errno = ENOSYS;
	return (-1);
}

int
eth_set(eth_t *e, const eth_addr_t *ea)
{
	errno = ENOSYS;
	return (-1);
}
