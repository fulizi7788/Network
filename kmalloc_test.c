/*
 * =====================================================================================
 *
 *       Filename:  kmalloc_test.c
 *
 *    Description:  kernel memery alloc test
 *
 *        Version:  1.0
 *        Created:  04/15/2014 01:33:09 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zhangcl (NO), chunlei1011@126.com
 *        Company:  iPanel TV inc.
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

MODULE_LICENSE("GPL");

#define memmap_trace(format, xargs...) printk("%s:%d:"format"\n", __FUNCTION__, __LINE__, ##xargs)
#define physical_base 0x40000000
#define remap_size    0x1000000
#define DEVICE_NUMBER 10

static int memdeepth = 0;
static int queennum = 0;
static int membase = 0;
static int major = 0;  /* default dynamic */
static struct cdev netfilter_cdev;

/* get param form userspace when insmod */
module_param(memdeepth, int, S_IRUSR);
module_param(queennum, int, S_IRUSR);
module_param(major, int, S_IRUSR);

static int netfilter_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t netfilter_read(struct file *file, char __user *data, size_t length, loff_t *ppos)
{
	return 0;
}

static ssize_t netfilter_write(struct file *file, const char __user *data, size_t length, loff_t *ppos)
{
	return 0;
}

static int netfilter_release(struct inode *inode, struct file *file)
{
	return 0;
}


static int netfilter_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pfn;
	unsigned long start = vma->vm_start; 
	unsigned long size = PAGE_ALIGN(vma->vm_end - vma->vm_start);

	//memmap_trace("hello world.");
	memmap_trace("...");
	if(size > remap_size || membase == 0)
	{
		return -EINVAL;
	}
	/* coculate physical memery address page offset */
	pfn = membase >> PAGE_SHIFT;

	memmap_trace("physical page offset:%d", pfn);

	return remap_pfn_range(vma, start, pfn, size, PAGE_SHARED);
}

static struct file_operations netfilter_fileops = {
	.owner = THIS_MODULE,
	.open = netfilter_open,
	.read = netfilter_read,
	.write = netfilter_write,
	.release = netfilter_release,
	.mmap = netfilter_mmap
};

/* module init fucntion */
static int mem_module_init(void)
{
	dev_t dev;
	int rc = 0;

	//printk("hello world.\n");
#if 1
	if(memdeepth)
	{
		memmap_trace("0x%08x", memdeepth);
		//membase = __va(physical_base);
#if 1
		membase = ioremap(physical_base, remap_size);
		if(membase == 0)
		{
			memmap_trace("ioremap error.\n");
			return -1;
		}
#endif
		memmap_trace("remap address:0x%08x\n", membase);
		memcpy((char *)membase, "hello world!", 13);
	}
	if(queennum)
		memmap_trace("0x%08x", queennum);
#endif
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
		major = MAJOR(dev);
		printk("major=%d\n", major);
	}
	if(rc < 0)
	{
		memmap_trace("register chardev region error:%d", rc);
		return rc;

	}
	/* init device */
	cdev_init(&netfilter_cdev, &netfilter_fileops);
	/* add device to driver list */
	cdev_add(&netfilter_cdev, dev, DEVICE_NUMBER);
	return 0;
}

/* module deinit */
static void mem_module_exit(void)
{
	printk("module exit.\n");
	/* delete device and release resource */
	cdev_del(&netfilter_cdev);
	unregister_chrdev_region(MKDEV(major,0), DEVICE_NUMBER);
}

module_init(mem_module_init);
module_exit(mem_module_exit);
