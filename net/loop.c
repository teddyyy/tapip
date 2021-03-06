#include "netif.h"
#include "ip.h"
#include "lib.h"

#define LOOPBACK_MTU		1500
#define LOOPBACK_IPADDR		0x0100007F	/* 127.0.0.1 */
#define LOOPBACK_NETMASK	0x000000FF	/* 255.0.0.0 */
#define LOOPBACK_IP6ADDR	"::1"
#define LOOPBACK_NET6MASK	0x80		/* 128 */

struct netdev *loop;

static int loop_dev_init(struct netdev *dev)
{
	struct in6_addr a6;
	char straddr[INET6_ADDRSTRLEN];

	/* init veth: information for our netstack */
	dev->net_mtu = LOOPBACK_MTU;
	dev->net_ipaddr = LOOPBACK_IPADDR;
	dev->net_mask = LOOPBACK_NETMASK;
	dbg("%s ip address: " IPFMT, dev->net_name, ipfmt(dev->net_ipaddr));
	dbg("%s netmask:    " IPFMT, dev->net_name, ipfmt(dev->net_mask));

	if (inet_pton(LOOPBACK_IP6ADDR, &a6) <= 0)
		perrx("IPv6 address format is somthing wrong");

	dev->net_ip6addr = a6;
	dev->net_6mask = LOOPBACK_NET6MASK;
	dbg("%s ipv6 address: %s/%d", dev->net_name,
		inet_ntop(&dev->net_ip6addr, straddr, sizeof(straddr)),
		dev->net_6mask);

	/* net stats have been zero */
	return 0;
}

static void loop_recv(struct netdev *dev, struct pkbuf *pkb)
{
	dev->net_stats.rx_packets++;
	dev->net_stats.rx_bytes += pkb->pk_len;
	net_in(dev, pkb);
}

static int loop_xmit(struct netdev *dev, struct pkbuf *pkb)
{
	get_pkb(pkb);
	/* loop back to itself */
	loop_recv(dev, pkb);
	dev->net_stats.tx_packets++;
	dev->net_stats.tx_bytes += pkb->pk_len;
	return pkb->pk_len;
}

static struct netdev_ops loop_ops = {
	.init = loop_dev_init,
	.xmit = loop_xmit,
	.exit = NULL,
};

void loop_init(void)
{
	loop = netdev_alloc("lo", &loop_ops);
}

void loop_exit(void)
{
	netdev_free(loop);
}

