/*
 * fw-none.c
 * 
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: fw-none.c,v 1.1 2001/10/11 04:14:55 dugsong Exp $
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "dnet.h"

fw_t *
fw_open(void)
{
	errno = EOPNOTSUPP;
	return (NULL);
}

int
fw_add(fw_t *f, struct fw_rule *rule)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
fw_delete(fw_t *f, struct fw_rule *rule)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
fw_loop(fw_t *f, fw_handler callback, void *arg)
{
	errno = EOPNOTSUPP;
	return (-1);
}

int
fw_close(fw_t *f)
{
	errno = EOPNOTSUPP;
	return (-1);
}
