/*
 * rand.h
 *
 * Pseudorandom number generation, based on OpenBSD arc4random().
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 * Copyright (c) 1996 David Mazieres <dm@lcs.mit.edu>
 *
 * $Id: rand.h,v 1.1 2002/03/29 06:05:37 dugsong Exp $
 */

#ifndef RAND_H
#define RAND_H

typedef struct rand_handle rand_t;

__BEGIN_DECLS
rand_t	*rand_open(void);

int	 rand_get(rand_t *r, void *buf, size_t len);
int	 rand_set(rand_t *r, const void *seed, size_t len);

uint8_t	 rand_uint8(rand_t *r);
uint16_t rand_uint16(rand_t *r);
uint32_t rand_uint32(rand_t *r);

rand_t	*rand_close(rand_t *r);

#endif /* RAND_H */
