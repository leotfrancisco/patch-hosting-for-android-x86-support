/* Shared library add-on to iptables to add NOTRACK target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"NOTRACK target v%s takes no options\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	return 0;
}

static void
final_check(unsigned int flags)
{
}

static
struct iptables_target notrack 
= {	.next = NULL,
	.name = "NOTRACK",
	.version = IPTABLES_VERSION,
	.size = IPT_ALIGN(0),
	.userspacesize = IPT_ALIGN(0),
	.help = &help,
	.init = &init,
	.parse = &parse,
	.final_check = &final_check,
	.print = NULL, /* print */
	.save = NULL, /* save */
	.extra_opts = opts
};

void ipt_NOTRACK_init(void)
{
	register_target(&notrack);
}
