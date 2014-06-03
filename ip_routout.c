/*
 * =====================================================================================
 *
 *       Filename:  ip_routout.c
 *
 *    Description:  send stolen packet out
 *
 *        Version:  1.0
 *        Created:  05/29/2014 09:43:12 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhangcl (NO), chunlei1011@126.com
 *        Company:  iPanel TV inc.
 *
 * =====================================================================================
 */
#include <net/snmp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/xfrm.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/arp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netlink.h>

static inline int skb_dst_mtu(struct sk_buff *skb)
{
	struct inet_sock *inet = skb->sk ? inet_sk(skb->sk) : NULL;

	return (inet && inet->pmtudisc == IP_PMTUDISC_PROBE) ?
	       skb_dst(skb)->dev->mtu : dst_mtu(skb_dst(skb));
}

static inline int ip_finish_routout(struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = (struct rtable *)dst;
	struct net_device *dev = dst->dev;
	unsigned int hh_len = LL_RESERVED_SPACE(dev);

	if (rt->rt_type == RTN_MULTICAST) {
		IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTMCAST, skb->len);
	} else if (rt->rt_type == RTN_BROADCAST)
		IP_UPD_PO_STATS(dev_net(dev), IPSTATS_MIB_OUTBCAST, skb->len);

	/* Be paranoid, rather than too clever. */
	if (unlikely(skb_headroom(skb) < hh_len && dev->header_ops)) {
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, LL_RESERVED_SPACE(dev));
		if (skb2 == NULL) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		kfree_skb(skb);
		skb = skb2;
	}

	if (dst->hh)
		return neigh_hh_output(dst->hh, skb);
	else if (dst->neighbour)
		return dst->neighbour->output(skb);

	if (net_ratelimit())
		printk(KERN_DEBUG "ip_finish_output2: No header cache and no neighbour!\n");
	kfree_skb(skb);
	return -EINVAL;
}

/** 
 * stolen packet at NF_INET_POST_ROUTING point
 * and send out use this function
 * */
int ip_routout(struct sk_buff *skb)
{
	printk("---------route.rout.start----------------------\n");
	printk("skb->mac_head=0x%08x\n", skb->mac_header);
	printk("skb->network_head=0x%08x\n", skb->network_header);
	printk("skb->transport_head=0x%08x\n", skb->transport_header);
	printk("skb->head=0x%08x\n", skb->head);
	printk("skb->data=0x%08x\n", skb->data);
	printk("skb->tail=0x%08x\n", skb->tail);
	printk("skb->endl=0x%08x\n", skb->end);
	printk("skb->true_len=%d\n", skb->len);
	printk("skb->data_len=%d\n", skb->data_len);

#if defined(CONFIG_NETFILTER) && defined(CONFIG_XFRM)
	/* Policy lookup after SNAT yielded a new pilicy */
	if (skb_dst(skb)->xfrm != NULL) {
		IPCB(skb)->flags |= IPSKB_REROUTED;
		return dst_output(skb);
	}
#endif
	if(skb->len > skb_dst_mtu(skb) && !skb_is_gso(skb))
	{
		return ip_fragment(skb, ip_finish_routout);
		}
	else
	{
		printk("---------route.rout.complete----------------------\n");
		return ip_finish_routout(skb);
		}
}
