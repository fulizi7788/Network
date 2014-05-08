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

#include "mem_pool.h"

MODULE_LICENSE("GPL");

/* Global Function */
extern int register_hook(void);
extern int unregister_hook(void);

/* Global Variable */
mem_region_t mem_region[MAX_REGION_NUM];

/* Local Variable */
static int memdeepth = 0;
static int region_num = 0;
static int membase = 0;
static int major = 0;  /* default dynamic */
static struct cdev mem_pool_cdev;
static int custom_mtu = 0;    /* net packet MTU */
static int custom_headl = 0;  /* net packet head length */

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
	int i = 0;

	pnode = (net_packet_t *)((unsigned char *)pregion->region_start + sizeof(mem_region_t));
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
		pnode->buf = (unsigned char *)pnode + sizeof(net_packet_t);
		/* init packet status */
		pnode->pack_stat = PACKET_FREE;
		/* init the packet list status */
		pnode->list_stat = LIST_FREE;
		/* add the node into free list in this region */
		list_add(&pnode->list, &pregion->free_list);
		/* caculate the next node start, next node immediately following this node
		 * next pnode = (insigned char *)pnode + sizeof(struct net_packet) + MTU + head length
		 * */
		pnode = (net_packet_t *)((unsigned char *)pnode + pregion->mtu + pregion->headl + sizeof(net_packet_t));
	}
}

/* init every region in memery pool */
static void mem_region_init(void)
{
	int pool_length = 0;
	int region_length = 0;
	int spare = 0;
	int i = 0;
	mem_region_t *pmr = (mem_region_t *)membase; /* memery remap base address */

/* only for test */
	mem_pool_trace("register net hook function.");
	register_hook();

	return;
/* end */

	/* memery align */
	spare = memdeepth % region_num;
	pool_length = memdeepth - spare;
	/* caclute every region length */
	region_length = pool_length / region_num;

	for(i = 0; i < region_num; i++)
	{
		mem_region[i].region_start = (int)pmr;
		mem_region[i].region_end = (int)((unsigned char *)pmr + region_length - 1);
		if(custom_mtu)
		{
			/* support custom config MTU and head length from ioctl */
			mem_region[i].mtu = custom_mtu;
			mem_region[i].headl = custom_headl;
		}
		else
		{
			/* mtu = Ethernet MTU */
			mem_region[i].mtu = ETHERNET_MTU;
			/* head length = Ethernet head length */
			mem_region[i].headl = ETHERNET_HEAD;
		}
		/* init free list in memery region */
		region_init(&mem_region[i], region_length);
	}
}

/* module init fucntion */
static int mem_module_init(void)
{
	dev_t dev;
	int rc = 0;

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
	return 0;
}

/* module deinit */
static void mem_module_exit(void)
{
	/* delete device and release resource */
	unregister_hook();
	cdev_del(&mem_pool_cdev);
	unregister_chrdev_region(MKDEV(major,0), DEVICE_NUMBER);
}

module_init(mem_module_init);
module_exit(mem_module_exit);
