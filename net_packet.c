/*
 * =====================================================================================
 *
 *       Filename:  net_packet.c
 *
 *    Description:  capture net packet, put into memery pool
 *
 *        Version:  1.0
 *        Created:  04/29/2014 02:31:06 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhangcl (NO), chunlei1011@126.com
 *        Company:  none
 *
 * =====================================================================================
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>


/* capture net packet function */
static int capture_packet(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, \
		const struct net_device *out, int (*okfn)(struct sk_buff *) );

/* hook operation */
static struct nf_hook_ops hook_pack={
	.hook = capture_packet, /* capture function */
	.owner = THIS_MODULE,
	.pf = PF_INET,          /* IPV4 */
	.hooknum = 4,			/* NF_IP_POST_ROUTING,  hook number */
	.priority = NF_IP_PRI_FILTER, /* priority */
};

/* capture net packet put in memery pool */
static int capture_packet(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, \
		const struct net_device *out, int (*okfn)(struct sk_buff *) )
{
	printk("skb->mac_head=0x%08x\n", skb->mac_header);
	printk("skb->network_head=0x%08x\n", skb->network_header);
	printk("skb->transport_head=0x%08x\n", skb->transport_header);
	printk("skb->head=0x%08x\n", skb->head);
	printk("skb->data=0x%08x\n", skb->data);
	printk("skb->tail=0x%08x\n", skb->tail);
	printk("skb->endl=0x%08x\n", skb->end);
	printk("skb->true_len=%d\n", skb->len);
	printk("skb->data_len=%d\n", skb->data_len);
	return 0;
}

/* register hook */
int register_hook(void)
{
	return nf_register_hook(&hook_pack);
}

/* unregister hook */
void unregister_hook(void)
{
	nf_unregister_hook(&hook_pack);
}
