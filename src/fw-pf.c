/*
 * fw-pf.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: fw-pf.c,v 1.11 2002/12/17 03:59:51 dugsong Exp $
 */

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dnet.h"

/*
 * XXX - cope with moving pf API
 *     $OpenBSD: pfvar.h,v 1.102 2002/11/23 05:16:58 mcbride Exp $
 *     $OpenBSD: pfvar.h,v 1.68 2002/04/24 18:10:25 dhartmei Exp $
 */
#if defined(DIOCBEGINADDRS)
# define PFRA_ADDR(ra)	(ra)->addr.addr.v4.s_addr
# define PFRA_MASK(ra)	(ra)->addr.mask.v4.s_addr
#elif defined(PFRULE_FRAGMENT)
/* OpenBSD 3.2 */
# define PFRA_ADDR(ra)	(ra)->addr.addr.v4.s_addr
# define PFRA_MASK(ra)	(ra)->mask.v4.s_addr
#else
/* OpenBSD 3.1 */
# define PFRA_ADDR(ra)	(ra)->addr.v4.s_addr
# define PFRA_MASK(ra)	(ra)->mask.v4.s_addr
#endif

struct fw_handle {
	int	fd;
};

static void
fr_to_pr(const struct fw_rule *fr, struct pf_rule *pr)
{
	memset(pr, 0, sizeof(*pr));
	
	strlcpy(pr->ifname, fr->fw_device, sizeof(pr->ifname));
	
	pr->action = (fr->fw_op == FW_OP_ALLOW) ? PF_PASS : PF_DROP;
	pr->direction = (fr->fw_dir == FW_DIR_IN) ? PF_IN : PF_OUT;
	pr->proto = fr->fw_proto;

	pr->af = AF_INET;
	PFRA_ADDR(&pr->src) = fr->fw_src.addr_ip;
	addr_btom(fr->fw_src.addr_bits, &(PFRA_MASK(&pr->src)), IP_ADDR_LEN);
	
	PFRA_ADDR(&pr->dst) = fr->fw_dst.addr_ip;
	addr_btom(fr->fw_dst.addr_bits, &(PFRA_MASK(&pr->dst)), IP_ADDR_LEN);
	
	switch (fr->fw_proto) {
	case IP_PROTO_ICMP:
		if (fr->fw_sport[1])
			pr->type = (u_char)(fr->fw_sport[0] &
			    fr->fw_sport[1]) + 1;
		if (fr->fw_dport[1])
			pr->code = (u_char)(fr->fw_dport[0] &
			    fr->fw_dport[1]) + 1;
		break;
	case IP_PROTO_TCP:
	case IP_PROTO_UDP:
		pr->src.port[0] = htons(fr->fw_sport[0]);
		pr->src.port[1] = htons(fr->fw_sport[1]);
		if (pr->src.port[0] == pr->src.port[1])
			pr->src.port_op = PF_OP_EQ;
		else
			pr->src.port_op = PF_OP_IRG;

		pr->dst.port[0] = htons(fr->fw_dport[0]);
		pr->dst.port[1] = htons(fr->fw_dport[1]);
		if (pr->dst.port[0] == pr->dst.port[1])
			pr->dst.port_op = PF_OP_EQ;
		else
			pr->dst.port_op = PF_OP_IRG;
		break;
	}
}

static int
pr_to_fr(const struct pf_rule *pr, struct fw_rule *fr)
{
	memset(fr, 0, sizeof(*fr));
	
	strlcpy(fr->fw_device, pr->ifname, sizeof(fr->fw_device));

	if (pr->action == PF_DROP)
		fr->fw_op = FW_OP_BLOCK;
	else if (pr->action == PF_PASS)
		fr->fw_op = FW_OP_ALLOW;
	else
		return (-1);
	
	fr->fw_dir = pr->direction == PF_IN ? FW_DIR_IN : FW_DIR_OUT;
	fr->fw_proto = pr->proto;

	if (pr->af != AF_INET)
		return (-1);
	
	fr->fw_src.addr_type = ADDR_TYPE_IP;
	addr_mtob(&(PFRA_MASK(&pr->src)), IP_ADDR_LEN, &fr->fw_src.addr_bits);
	fr->fw_src.addr_ip = PFRA_ADDR(&pr->src);
	
 	fr->fw_dst.addr_type = ADDR_TYPE_IP;
	addr_mtob(&(PFRA_MASK(&pr->dst)), IP_ADDR_LEN, &fr->fw_dst.addr_bits);
	fr->fw_dst.addr_ip = PFRA_ADDR(&pr->dst);
	
	switch (fr->fw_proto) {
	case IP_PROTO_ICMP:
		if (pr->type) {
			fr->fw_sport[0] = pr->type - 1;
			fr->fw_sport[1] = 0xff;
		}
		if (pr->code) {
			fr->fw_dport[0] = pr->code - 1;
			fr->fw_dport[1] = 0xff;
		}
		break;
	case IP_PROTO_TCP:
	case IP_PROTO_UDP:
		fr->fw_sport[0] = ntohs(pr->src.port[0]);
		fr->fw_sport[1] = ntohs(pr->src.port[1]);
		if (pr->src.port_op == PF_OP_EQ)
			fr->fw_sport[1] = fr->fw_sport[0];

		fr->fw_dport[0] = ntohs(pr->dst.port[0]);
		fr->fw_dport[1] = ntohs(pr->dst.port[1]);
		if (pr->dst.port_op == PF_OP_EQ)
			fr->fw_dport[1] = fr->fw_dport[0];
	}
	return (0);
}

fw_t *
fw_open(void)
{
	fw_t *fw;

	if ((fw = calloc(1, sizeof(*fw))) == NULL)
		return (NULL);

	if ((fw->fd = open("/dev/pf", O_RDWR)) < 0)
		return (fw_close(fw));
	
	return (fw);
}

int
fw_add(fw_t *fw, const struct fw_rule *rule)
{
	struct pfioc_changerule pcr;
	
	assert(fw != NULL && rule != NULL);
	fr_to_pr(rule, &pcr.newrule);
	pcr.action = PF_CHANGE_ADD_TAIL;
	
	return (ioctl(fw->fd, DIOCCHANGERULE, &pcr));
}

int
fw_delete(fw_t *fw, const struct fw_rule *rule)
{
	struct pfioc_changerule pcr;
	
	assert(fw != NULL && rule != NULL);
	fr_to_pr(rule, &pcr.oldrule);
	pcr.action = PF_CHANGE_REMOVE;

	return (ioctl(fw->fd, DIOCCHANGERULE, &pcr));
}

int
fw_loop(fw_t *fw, fw_handler callback, void *arg)
{
	struct pfioc_rule pr;
	struct fw_rule fr;
	uint32_t n, max;
	int ret = 0;
	
	if (ioctl(fw->fd, DIOCGETRULES, &pr) < 0)
		return (-1);
	
	for (n = 0, max = pr.nr; n < max; n++) {
		pr.nr = n;
		
		if ((ret = ioctl(fw->fd, DIOCGETRULE, &pr)) < 0)
			break;
		if (pr_to_fr(&pr.rule, &fr) < 0)
			continue;
		if ((ret = callback(&fr, arg)) != 0)
			break;
	}
	return (ret);
}

fw_t *
fw_close(fw_t *fw)
{
	assert(fw != NULL);

	if (fw->fd > 0)
		close(fw->fd);
	free(fw);
	return (NULL);
}
