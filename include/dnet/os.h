/*
 * os.h
 *
 * Sleazy OS-specific defines.
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * $Id: os.h,v 1.2 2001/10/12 06:49:48 dugsong Exp $
 */

#ifndef DNET_OS_H
#define DNET_OS_H

/* XXX - require POSIX */
#include <sys/param.h>

/* XXX - Linux <feature.h>, IRIX <sys/endian.h>, etc. */
#include <netinet/in.h>
#include <arpa/inet.h>

#define DNET_LIL_ENDIAN		1234
#define DNET_BIG_ENDIAN		4321

/* BSD and IRIX */
#ifdef BYTE_ORDER
#if BYTE_ORDER == LITTLE_ENDIAN
# define DNET_BYTESEX		DNET_LIL_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
# define DNET_BYTESEX		DNET_BIG_ENDIAN
#endif
#endif

/* Linux */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __LITTLE_ENDIAN
# define DNET_BYTESEX		DNET_LIL_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
# define DNET_BYTESEX		DNET_BIG_ENDIAN
#endif
#endif

/* Solaris */
#if defined(_BIT_FIELDS_LTOH)
# define DNET_BYTESEX		DNET_LIL_ENDIAN
#elif defined (_BIT_FIELDS_HTOL)
# define DNET_BYTESEX		DNET_BIG_ENDIAN
#endif
#if defined(_SYS_INT_TYPES_H) || defined(__INTTYPES_INCLUDED)
# define u_int64_t		uint64_t
# define u_int32_t		uint32_t
# define u_int16_t		uint16_t
# define u_int8_t		uint8_t
#endif

/* Nastiness from old BIND code. */
#ifndef DNET_BYTESEX
# if defined(vax) || defined(ns32000) || defined(sun386) || defined(i386) || \
    defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
    defined(__alpha__) || defined(__alpha)
#  define DNET_BYTESEX		DNET_LIL_ENDIAN
# elif defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
    defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
    defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
    defined(apollo) || defined(__convex__) || defined(_CRAY) || \
    defined(__hppa) || defined(__hp9000) || \
    defined(__hp9000s300) || defined(__hp9000s700) || \
    defined (BIT_ZERO_ON_LEFT) || defined(m68k)
#  define DNET_BYTESEX		DNET_BIG_ENDIAN
# else
#  error "bytesex unknown"
# endif
#endif

#endif /* DNET_OS_H */
