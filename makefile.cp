# if KERNELRELEASE is defined, we've been ivoked from
# the kernel build system and can use its language

ifneq($(KERNELRELEASE),1)
	obj-m:=hello.o
# otherwise we called directly from the command line;
# invoke the kernel build system

else
	obj-m:=hello.o
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

all:
	$(MAKE) -C $(KERNELDIR) $(PWD) modules

clean:
	rm -rf *.cmd *.o *.ko *.mod.c *.order *.symvers
