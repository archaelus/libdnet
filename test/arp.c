/*
 * arp.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@monkey.org>
 *
 * $Id: arp.c,v 1.1 2001/10/11 04:14:45 dugsong Exp $
 */

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "dnet.h"

static void
usage(void)
{
	fprintf(stderr, "Usage: arp show\n"
	                "Usage: arp get host\n"
	                "Usage: arp add host mac\n"
	                "Usage: arp delete host\n"
	                "Usage: arp flush\n");
	exit(1);
}

static int
print_arp(struct addr *pa, struct addr *ha, void *arg)
{
	printf("%s at %s\n", addr_ntoa(pa), addr_ntoa(ha));
	return (0);
}

static int
delete_arp(struct addr *pa, struct addr *ha, void *arg)
{
	arp_t *a = (arp_t *)arg;

	if (arp_delete(a, pa) < 0)
		return (-1);
	
	return (print_arp(pa, ha, NULL));
}

int
main(int argc, char *argv[])
{
	struct addr pa, ha;
	arp_t *a;
	char *cmd;

	if (argc < 2)
		usage();

	cmd = argv[1];

	if ((a = arp_open()) == NULL)
		err(1, "arp_open");
	
	if (strcmp(cmd, "show") == 0) {
		if (arp_loop(a, print_arp, NULL) < 0)
			err(1, "arp_loop");
	} else if (strcmp(cmd, "get") == 0) {
		if (addr_pton(argv[2], &pa) < 0)
			err(1, "addr_pton");
		
		if (arp_get(a, &pa, &ha) < 0)
			err(1, "arp_get");

		print_arp(&pa, &ha, NULL);
	} else if (strcmp(cmd, "add") == 0) {
		if (addr_pton(argv[2], &pa) < 0 || addr_pton(argv[3], &ha) < 0)
			err(1, "addr_pton");

		if (arp_add(a, &pa, &ha) < 0)
			err(1, "arp_add");

		printf("%s added\n", addr_ntoa(&pa));
	} else if (strcmp(cmd, "delete") == 0) {
		if (addr_pton(argv[2], &pa) < 0)
			err(1, "addr_pton");

		if (arp_delete(a, &pa) < 0)
			err(1, "arp_delete");

		printf("%s deleted\n", addr_ntoa(&pa));
	} else if (strcmp(cmd, "flush") == 0) {
		if (arp_loop(a, delete_arp, a) < 0)
			err(1, "arp_loop");
	} else
		usage();

	arp_close(a);

	exit(0);
}
