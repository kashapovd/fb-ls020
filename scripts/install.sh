#!/bin/bash

# Скрипт установки драйвера LS020 для Orange Pi Zero 2W
# Использование: sudo ./install.sh

set -e

echo "LS020 Driver Installation Script"
echo "================================"

# Проверка прав root
if [ "$EUID" -ne 0 ]; then 
    echo "Запустите скрипт с правами root: sudo ./install.sh"
    exit 1
fi

# Проверка наличия заголовков ядра
KERNEL_VER=$(uname -r)
echo "Версия ядра: $KERNEL_VER"

if [ ! -d "/lib/modules/$KERNEL_VER/build" ]; then
    echo "Заголовки ядра не найдены!"
    echo "Установите их командой:"
    echo "sudo apt update && sudo apt install linux-headers-$KERNEL_VER"
    exit 1
fi

# Компиляция device tree overlay
echo "Компиляция Device Tree Overlay..."
if command -v dtc &> /dev/null; then
    dtc -@ -I dts -O dtb -o ls020-overlay.dtbo ls020-overlay.dts
    echo "Device Tree Overlay скомпилирован"
else
    echo "ВНИМАНИЕ: dtc не найден. Overlay не скомпилирован."
    echo "Установите device-tree-compiler: sudo apt install device-tree-compiler"
fi

# Компиляция драйвера
echo "Компиляция драйвера..."
make clean
make

if [ ! -f "ls020_fb.ko" ]; then
    echo "ОШИБКА: Компиляция драйвера не удалась!"
    exit 1
fi

echo "Драйвер успешно скомпилирован"

# Установка драйвера
echo "Установка драйвера..."
make install

# Загрузка overlay (если скомпилирован)
if [ -f "ls020-overlay.dtbo" ]; then
    echo "Копирование overlay..."
    cp ls020-overlay.dtbo /boot/overlays/ 2>/dev/null || \
    cp ls020-overlay.dtbo /boot/dtb/overlays/ 2>/dev/null || \
    echo "ВНИМАНИЕ: Не удалось скопировать overlay. Скопируйте ls020-overlay.dtbo вручную в директорию overlays"
    
    echo ""
    echo "Добавьте в /boot/armbianEnv.txt строку:"
    echo "overlays=ls020-overlay"
    echo "или в /boot/config.txt:"
    echo "dtoverlay=ls020-overlay"
fi

echo ""
echo "Установка завершена!"
echo ""
echo "Для использования драйвера:"
echo "1. Подключите дисплей согласно распиновке в ls020-overlay.dts"
echo "2. Перезагрузите систему"
echo "3. Загрузите модуль: sudo modprobe ls020_fb"
echo "4. Проверьте: ls /dev/fb*"
echo ""
echo "Пример использования:"
echo "cat /dev/urandom > /dev/fb1  # заливка случайными данными"
echo "echo 0 > /sys/class/graphics/fb1/blank  # включить дисплей"
echo ""
echo "Для отладки:"
echo "dmesg | grep ls020  # проверить логи драйвера"