obj-m := m_slave.o
m_slave-y := slave_mass_storage.o
obj-m += usb_f_mass_storage.o 
usb_f_mass_storage-y := f_mass_storage.o storage_common.o

KDIR=/home/elinux/linux-4.4.96

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

# make ARCH=arm CROSS_COMPILE=arm-linux-
