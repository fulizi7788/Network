KERNELDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

obj-m:=kmalloc_test.o

#kmalloc_test-objs:kmalloc_test.o

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.mod.c *.order *.symvers *.ko *.swo *.swp
