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

MODULE_LICENSE("GPL");

#define memmap_trace(format, xargs...) printk("%s:%d:"format"\n", __FUNCTION__, __LINE__, ##xargs)
#define MEMBASEADDR 0x80000000

int memdeepth= 0;
int queenlength= 0;

module_param(memdeepth, int, S_IRUSR);
module_param(queenlength, int, S_IRUSR);

static int hello_module_init(void)
{
	printk("hello world.\n");
	if(memdeepth)
	{
		memmap_trace("0x%08x", memdeepth);
		
		
	}
	if(queenlength)
		memmap_trace("0x%08x", queenlength);
	return 0;
}

static void hello_module_exit(void)
{
	printk("good bye world.\n");
}

module_init(hello_module_init);
module_exit(hello_module_exit);
