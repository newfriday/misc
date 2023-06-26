obj-m += ivshmem-drv.o
KERNELDIR := /lib/modules/$(shell uname -r)/build/
PWD := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.ko *.mod* *.o modules.* Module.symvers
