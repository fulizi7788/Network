#ifndef __MEM_POOL_H__
#define __MEM_POOL_H__

#define mem_pool_trace(format, xargs...) printk("%s:%d:"format"\n", __FUNCTION__, __LINE__, ##xargs)
#define physical_base 0x40000000
#define remap_size    0x1000000
#define DEVICE_NUMBER  10
#define MAX_REGION_NUM 10
#define ETHERNET_MTU   1500 /* Default Ethernet MTU */
#define ETHERNET_HEAD  26 /* 8bytes synchronisation filed + 6bytes dest mac + 6bytes src mac + 2bytes protocol + 4bytes CRC */

typedef enum npstat{
	PACKET_PREPROCESS,   /* wait process */
	PACKET_TRANSMIT,     /* can transmit */
	PACKET_DROP,          /* should be dropped */
	PACKET_FREE
}npstat_e;

typedef enum list_stat{
	LIST_PREPROCESS,     /* list wait process */ 
	LIST_FREE            /* list free */
}list_stat_e;

/* memery region manager struct */
typedef struct mem_region{
	int region_start; /* memery region start address */
	int region_end;   /* memery region end address */
	int mtu;          /* max transmit unit size */
	int headl;         /* net packet head length */
	int unit_num;     /* this region have packet numbers */
	struct list_head free_list; /* free list head, manager free space in this memery region */
	struct list_head process_list; /* save rcv net packet in these list, wait user space process */
	//struct list_head processed_list;  /* processed list, wait transmit or drop */
}mem_region_t;

typedef struct net_packet{
	struct list_head list; /* packet list */
	unsigned int packet_number; /* packet count */
	unsigned int sort_number;   /* put this packet in which region based this number  */
	unsigned int buf_len;  /* data length */
	unsigned char *buf;    /* net packet data */
	npstat_e pack_stat;    /* packet status */
	list_stat_e list_stat; /* current list status */
	struct net_device *out; /* packet forward out from the out device */
}net_packet_t;
#endif
