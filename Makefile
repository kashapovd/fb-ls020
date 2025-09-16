obj-m += ls020_fb.o

KERNEL_VERSION := $(shell uname -r)
KERNEL_SRC ?= /lib/modules/$(KERNEL_VERSION)/build

ifeq ($(wildcard $(KERNEL_SRC)),)
    KERNEL_SRC := /usr/src/linux-headers-$(KERNEL_VERSION)
endif

TEST_BINARY = test_lcd
TEST_SOURCES = test_lcd.c

all: module app

module:
	make -C $(KERNEL_SRC) M=$(PWD) modules

app: $(TEST_BINARY)

$(TEST_BINARY): $(TEST_SOURCES)
	gcc -o $(TEST_BINARY) $(TEST_SOURCES) -std=c99

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
	rm -f $(TEST_BINARY)

install: module
	sudo make -C $(KERNEL_SRC) M=$(PWD) modules_install
	sudo depmod -A

test: app
	sudo ./$(TEST_BINARY)

reload:
	sudo sh -c "echo spi1.0 > /sys/bus/spi/drivers/ls020_fb/unbind"
	sudo rmmod ls020_fb
	sudo cp ls020_fb.ko /lib/modules/$(KERNEL_VERSION)/kernel/drivers/video/fbdev/
	sudo depmod -a
	sudo insmod ls020_fb.ko rotation=0 fps=60

.PHONY: all module app clean install test reload