/*
 * =====================================================================================
 *
 *       Filename:  mem_pool.c
 *
 *    Description:  memery pool manager
 *
 *        Version:  1.0
 *        Created:  04/15/2014 01:33:09 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhangcl (NO), chunlei1011@126.com
 *        Company:  none
 *
 * =====================================================================================
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>

#include "mem_pool.h"

MODULE_LICENSE("GPL");

/* Global Function */
extern int register_hook(void);
extern int unregister_hook(void);
extern int ip_routout(struct sk_buff *skb);

/* Global Variable */
mem_region_t *mem_region[MAX_REGION_NUM];
int volatile post_rout = 0;
int region_num = 0;
DECLARE_WAIT_QUEUE_HEAD(skb_wait);
DECLARE_MUTEX(list_sem);

/* Local Variable */
static int memdeepth = 0;
static int membase = 0;
static int major = 0;  /* default dynamic */
static struct cdev mem_pool_cdev;
static int custom_mtu = 0;    /* net packet MTU */
static int custom_headl = 0;  /* net packet head length */
static struct task_struct *kthread;

/* get param form userspace when insmod */
module_param(memdeepth, int, S_IRUSR);
module_param(region_num, int, S_IRUSR);
module_param(major, int, S_IRUSR);

static int mem_pool_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mem_pool_read(struct file *file, char __user *data, size_t length, loff_t *ppos)
{
	return 0;
}

static ssize_t mem_pool_write(struct file *file, const char __user *data, size_t length, loff_t *ppos)
{
	return 0;
}

static int mem_pool_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations mem_pool_fileops = {
	.owner = THIS_MODULE,
	.open = mem_pool_open,
	.read = mem_pool_read,
	.write = mem_pool_write,
	.release = mem_pool_release,
};

/* init the free list in memery region */
static void region_init(mem_region_t *region, int region_length)
{
	mem_region_t *pregion = region;
	int length = 0;
	int packet_length = 0;
	int align_length = 0;
	int unit_num = 0;
	net_packet_t *pnode = NULL;
	unsigned char *paddr = NULL;
	int i = 0;

	paddr = pregion->region_start + sizeof(mem_region_t);
	/* packet memery length = region length - (struct  memery region length) */
	length = region_length - sizeof(mem_region_t);
	/* caculate packet length */
	packet_length = pregion->mtu + pregion->headl;
	packet_length += sizeof(net_packet_t);
	/* align */
	align_length = length - (length%packet_length);
	/* caculate unit sum */
	unit_num = align_length / packet_length;

	/* init list head */
	INIT_LIST_HEAD(&pregion->free_list);
	INIT_LIST_HEAD(&pregion->process_list);

	/* init the free list */	
	for(i = 0; i < unit_num; i++)
	{
		/* the packet buf immediately following the struct */
		pnode = (net_packet_t *)paddr;
		pnode->buf = paddr + sizeof(net_packet_t);
		printk("pnode->buf=0x%08x\n", pnode->buf);
		/* init packet status */
		pnode->pack_stat = PACKET_FREE;
		/* init the packet list status */
		pnode->list_stat = LIST_FREE;
		/* add the node into free list in this region */
		list_add(&pnode->list, &pregion->free_list);
		/* caculate the next node start, next node immediately following this node
		 * next pnode = (insigned char *)pnode + sizeof(struct net_packet) + MTU + head length
		 * */
		paddr = paddr + pregion->mtu + pregion->headl + sizeof(net_packet_t);
	}
}

/* init every region in memery pool */
static void mem_region_init(void)
{
	int pool_length = 0;
	int region_length = 0;
	int spare = 0;
	int i = 0;
	unsigned char *pmr = (unsigned char *)membase; /* memery remap base address */
	mem_region_t *pregion = NULL;


	/* memery align */
	spare = memdeepth % region_num;
	pool_length = memdeepth - spare;
	/* caclute every region length */
	region_length = pool_length / region_num;

	for(i = 0; i < region_num; i++)
	{
		mem_region[i] = (mem_region_t *)pmr;
		pregion = mem_region[i];
		pregion->region_start = pmr;
		pregion->region_end = pmr + region_length;

		if(custom_mtu)
		{
			/* support custom config MTU and head length from ioctl */
			pregion->mtu = custom_mtu;
			pregion->headl = custom_headl;
		}
		else
		{
			/* mtu = Ethernet MTU */
			pregion->mtu = ETHERNET_MTU;
			/* head length = Ethernet head length */
			pregion->headl = ETHERNET_HEAD;
		}
		/* init free list in memery region */
		region_init(pregion, region_length);
		pmr += region_length;
	}
}

static int post_routing_task(void *unused)
{
	struct list_head *proc_head = NULL;
	struct list_head *free_head = NULL;
	struct list_head *plist = NULL;
	mem_region_t *preg = NULL;
	int i = 0;
	net_packet_t *pnet = NULL;
	int frags = 0;

	while(!kthread_should_stop())
	{
		wait_event_interruptible(skb_wait, (post_rout != 0) || kthread_should_stop());
		if(i < region_num) 
		{
			/* every region */
			preg = mem_region[i];
			/* process list in every region */
			proc_head = &preg->process_list;
			free_head = &preg->free_list;
			if(proc_head->next == proc_head)
			{
				i++;
				printk("region %d empty.\n", i);
				continue;
			}
			plist = proc_head->next;
			/* traverse the process list in every region */
			down(&list_sem);
			while(plist->next != plist)
			{
				pnet = list_entry(plist, net_packet_t, list);
				frags = pnet->last_frag_num;
				//printk("<0>frags number %d.\n", frags);
				ip_routout(pnet->skb);
				do{
					if (pnet == NULL)
						pnet = list_entry(plist, net_packet_t, list);
					plist = plist->next;
					list_del(&pnet->list);
					printk("region %d free_head=0x%08x\n", i, free_head);
					list_add_tail(&pnet->list, free_head);
					pnet = NULL;
					frags--;
				}while(frags >= 0);
			}
			up(&list_sem);
			i++;
		}
		else
		{
			post_rout = 0;
			i = 0;
		}
	}
	return 0;
}

/* module init fucntion */
static int mem_module_init(void)
{
	dev_t dev;
	int rc = 0;

	printk("hello world.\n");

	if(memdeepth)
	{
		mem_pool_trace("0x%08x", memdeepth);
		/* remap physical memery address to virtual address */
		membase = (int)ioremap(physical_base, memdeepth);
		if(membase == 0)
		{
			mem_pool_trace("ioremap error.\n");
			return -1;
		}
		mem_pool_trace("remap address:0x%08x\n", membase);
	}
	/* the number of the memery pool division */
	if(region_num)
		mem_pool_trace("0x%08x", region_num);
	mem_region_init();
	if(major)
	{
		/* make device(id) */
		dev = MKDEV(major, 0);
		/* static register device region */
		rc = register_chrdev_region(dev, DEVICE_NUMBER, "ntf");
	}
	else
	{
		/* dynamic alloc device region */
		rc = alloc_chrdev_region(&dev, 0, DEVICE_NUMBER, "ntf");
		/* major */
		major = MAJOR(dev);
		printk("major=%d\n", major);
	}
	if(rc < 0)
	{
		mem_pool_trace("register chardev region error:%d", rc);
		return rc;

	}
	/* init device */
	cdev_init(&mem_pool_cdev, &mem_pool_fileops);
	/* add device to driver list */
	cdev_add(&mem_pool_cdev, dev, DEVICE_NUMBER);

	/* only for test */
	mem_pool_trace("register net hook function.");
	register_hook();
	/* end */

	/* start post routing task */
	kthread = kthread_run(post_routing_task, NULL, "postrouting");
	
	return 0;
}

/* module deinit */
static void mem_module_exit(void)
{
	printk("<0>good bye world.\n");
#if 1
	/* delete device and release resource */
	kthread_stop(kthread);
	unregister_hook();
	cdev_del(&mem_pool_cdev);
	unregister_chrdev_region(MKDEV(major,0), DEVICE_NUMBER);
	iounmap(membase);
#endif
}

module_init(mem_module_init);
module_exit(mem_module_exit);
