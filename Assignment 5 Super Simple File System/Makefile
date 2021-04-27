CC=gcc
LOCAL_CFLAGS=-Wall -Werror

obj-m += s2fs.o

all: s2fs.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: s2fs.ko
	sudo insmod $<

uninstall: s2fs.ko
	sudo rmmod $<
