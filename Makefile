KERNELDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

obj-m:=net_protect.o
net_protect-objs:=mem_pool.o 
net_protect-objs+=net_packet.o

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.mod.c *.order *.symvers *.ko *.swo .*.swp .*.cmd .*.d
