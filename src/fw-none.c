/*
 * fw-none.c
 * 
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: fw-none.c,v 1.2 2002/01/06 22:00:01 dugsong Exp $
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "dnet.h"

fw_t *
fw_open(void)
{
	errno = ENOSYS;
	return (NULL);
}

int
fw_add(fw_t *f, struct fw_rule *rule)
{
	errno = ENOSYS;
	return (-1);
}

int
fw_delete(fw_t *f, struct fw_rule *rule)
{
	errno = ENOSYS;
	return (-1);
}

int
fw_loop(fw_t *f, fw_handler callback, void *arg)
{
	errno = ENOSYS;
	return (-1);
}

int
fw_close(fw_t *f)
{
	errno = ENOSYS;
	return (-1);
}
