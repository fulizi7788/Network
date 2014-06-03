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
#include <linux/wait.h>
#include <linux/sched.h>

#include "mem_pool.h"

/* gloable variable */
extern mem_region_t mem_region[MAX_REGION_NUM];
extern volatile int post_rout;
extern int region_num;
extern wait_queue_head_t skb_wait;
extern struct semaphore list_sem;

/* local variable */
int packet_num = 0;

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

static bool check_free(int num, struct list_head *head)
{
	struct list_head *plist;
	int i = 0;

	plist = head->next;

	/* check enough? the free element */
	for(i = 0; i < num; i++)
	{
		if(plist->next == head)
			return false;
		plist = plist->next;
		
	}
	return true;
}

/* retrun available region number for store the net packet */
static int search_region(struct sk_buff *skb)
{
	int regnum = 0;
	mem_region_t *pregion = NULL;
	struct list_head *head;
	int data_len = 0;
	int space = 0;
	int need;
	int sort_num;

	/* set sort number, packet_num is a serial number */
	sort_num = packet_num;
	/* caculate the region number data will be put in */
	regnum = sort_num %  region_num;
	/* average put the packet into every region */
	pregion = &mem_region[regnum];
	head = &pregion->free_list;
	/* space in every element */
	space = pregion->mtu + pregion->headl;
	/* data length in this net packet */
	data_len = skb->len;

	/* the number element needed in free list */
	need = data_len / space;
	if(data_len % space)
		need += 1;

	printk("need %d free node to store this packet.\n", need);

	if(check_free(need, head)) 
		return regnum;
	for( sort_num = 0; sort_num < region_num; sort_num++)
	{
		if(sort_num == regnum)
			continue;
		pregion = &mem_region[sort_num];
		head = &pregion->free_list;
		if(check_free(need, head))
			break;
	}
	return sort_num;
}

/* store packet in memery region's free list */
static void store_packet(int regnum, struct sk_buff *skb)
{
	mem_region_t *pregion = NULL;
	struct list_head *free_head;
	struct list_head *proc_head;
	int data_len = 0;
	int space = 0;
	int frags = 0;
	int i = 0;
	struct list_head *plist = 0;
	net_packet_t *pnet;
	unsigned char *pdata = skb->data;
	
	pregion = &mem_region[regnum];
	free_head = &pregion->free_list;
	proc_head = &pregion->process_list;
	/* caculate free space in every element in free list */
	space = pregion->mtu + pregion->headl;
	/* get lenght in net packet */
	data_len = skb->len;

	/* need element numbers */
	frags = data_len / space;
	if(data_len % space)
		frags += 1;

	//printk("this packet will be frags %d space %d.\n", frags, space);

	plist = free_head->next;
	down(&list_sem);
	for(i = 0; i < frags; i++)
	{
		pnet = list_entry(plist, net_packet_t, list);
		pnet->frag_num = i;
		pnet->last_frag_num = frags - 1;
		pnet->skb = skb; /* save the skb address, for post_routing_task */
		if(data_len >= space)
		{
			/* copy data into region free list */
			memcpy(pnet->buf, pdata, space);
			pnet->data_len = space;
			data_len -= space;
		}
		else
		{
			if(pnet->buf == NULL)
			{
				printk("unknow error plist=0x%08x.\n", pnet);
				break;
			}
			/* copy data into region free list */
			memcpy(pnet->buf, pdata, data_len);
			pnet->data_len = data_len;
			data_len = 0;
		}
		printk("data_len=%d\n", data_len);	
		plist = plist->next;
		if(plist->next == NULL)
		{
			printk("no available node to store packet, error.\n");
			break;
		}
		/* delete this node from free list */
		list_del(&pnet->list);
		/* add the node into process list tail */
		list_add_tail(&pnet->list, proc_head);
	}
	up(&list_sem);
	post_rout = 1;
	if(post_rout)
	{
		wake_up(&skb_wait);
	}
	printk("frags: %d\n", pnet->last_frag_num);
	printk("store packet in region %d success.\n", regnum);
}

/* capture net packet put in memery pool */
static int capture_packet(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, \
		const struct net_device *out, int (*okfn)(struct sk_buff *) )
{
	int regnum = 0;

#if 1
	printk("skb->mac_head=0x%08x\n", skb->mac_header);
	printk("skb->network_head=0x%08x\n", skb->network_header);
	printk("skb->transport_head=0x%08x\n", skb->transport_header);
	printk("skb->head=0x%08x\n", skb->head);
	printk("skb->data=0x%08x\n", skb->data);
	printk("skb->tail=0x%08x\n", skb->tail);
	printk("skb->endl=0x%08x\n", skb->end);
	printk("skb->true_len=%d\n", skb->len);
	printk("skb->data_len=%d\n", skb->data_len);
#endif
	/* skb is no linearize */
	if(skb_is_nonlinear(skb))
	{
		/* linearize the skb */
		if(skb_linearize(skb))
		{
			/* failed return */
			return  NF_DROP;
		}
	}
	/* search available region to store this packet */
	regnum = search_region(skb);
	printk("find available regnum %d to store this packet.\n", regnum);
	if(regnum == MAX_REGION_NUM)
	{
		/* no free memery to store this packet, just drop */
		printk("No free memery to store this packet.\n");
		return NF_DROP;
	}
	/* sotre packet into memery region */
	store_packet(regnum, skb);
	packet_num++;
#if 1
	/* now only for test */
	return NF_STOLEN;
#else
	return NF_ACCEPT;
#endif
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
