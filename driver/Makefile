obj-m += pulsecnt.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all: module tool

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

tool: pulsecnt_ctl

pulsecnt_ctl: pulsecnt_ctl.c pulsecnt_ioctl.h
	$(CC) -Wall -Wextra -O2 -o $@ pulsecnt_ctl.c

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f pulsecnt_ctl

.PHONY: all module tool clean
