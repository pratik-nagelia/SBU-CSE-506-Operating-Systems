CC=gcc
LOCAL_CFLAGS=-Wall -Werror

obj-m += perftop.o

all: perftop.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: perftop.ko
	sudo insmod $<

uninstall: perftop.ko
	sudo rmmod $<
