# Makefile для компиляции модуля ядра и тестового приложения

# Модуль ядра
obj-m += ls020_fb.o

# Определение пути к заголовкам ядра
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Если путь не существует, попробуем альтернативный
ifeq ($(wildcard $(KERNEL_SRC)),)
    KERNEL_SRC := /usr/src/linux-headers-$(shell uname -r)
endif

# Тестовое приложение
TEST_BINARY = test_fb
TEST_SOURCES = test_fb.c

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

load:
	sudo modprobe ls020_fb

unload:
	sudo modprobe -r ls020_fb

test: app
	sudo ./$(TEST_BINARY)

reload:
	sudo sh -c "echo spi1.0 > /sys/bus/spi/drivers/ls020_fb/unbind"
	sudo rmmod ls020_fb
	sudo insmod ./ls020_fb.ko

# Компиляция device tree overlay
dtbo: ls020-overlay.dtbo

ls020-overlay.dtbo: ls020-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<

.PHONY: all module app clean install load unload test dtbo