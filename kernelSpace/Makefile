obj-m += memefs_module.o

KDIR = /usr/src/project-3-xchequer

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
