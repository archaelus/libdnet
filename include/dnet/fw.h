/*
 * fw.h
 *
 * Network firewalling operations.
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: fw.h,v 1.1 2001/10/11 04:14:49 dugsong Exp $
 */

#ifndef DNET_FW_H
#define DNET_FW_H

struct fw_rule {
	char		device[14];	/* interface name */
	u_char		op:4,		/* operation */
#define FW_OP_ALLOW	1
#define FW_OP_BLOCK	2
			direction:4;	/* direction */
#define FW_DIR_IN	1
#define FW_DIR_OUT	2
	u_char		proto;		/* IP protocol */
	struct addr	src;		/* src address / net */
	struct addr	dst;		/* dst address / net */
	u_short		sport[2];	/* range or ICMP type / mask */
	u_short		dport[2];	/* range or ICMP code / mask */
};

typedef struct fw_handle fw_t;

typedef int (*fw_handler)(struct fw_rule *rule, void *arg);

fw_t	*fw_open(void);
int	 fw_add(fw_t *f, struct fw_rule *rule);
int	 fw_delete(fw_t *f, struct fw_rule *rule);
int	 fw_loop(fw_t *f, fw_handler callback, void *arg);
int	 fw_close(fw_t *f);

#endif /* DNET_FW_H */
