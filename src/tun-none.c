/*
 * tun-none.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: tun-none.c,v 1.1 2004/09/10 02:35:51 dugsong Exp $
 */

#include "config.h"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "dnet.h"

tun_t *
tun_open(struct addr *src, struct addr *dst, int mtu)
{
	errno = ENOSYS;
	return (NULL);
}

const char *
tun_name(tun_t *tun)
{
	errno = ENOSYS;
	return (NULL);
}

int
tun_fileno(tun_t *tun)
{
	errno = ENOSYS;
	return (-1);
}

size_t
tun_send(tun_t *tun, const void *buf, size_t size)
{
	errno = ENOSYS;
	return (-1);
}

size_t
tun_recv(tun_t *tun, void *buf, size_t size)
{
	errno = ENOSYS;
	return (-1);
}

tun_t *
tun_close(tun_t *tun)
{
	return (NULL);
}
