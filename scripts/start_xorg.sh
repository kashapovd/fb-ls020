#!/bin/bash

echo "=== Starting Xorg on LS020 Display ==="

# Проверить что framebuffer существует
if [ ! -c /dev/fb0 ]; then
    echo "ERROR: /dev/fb0 not found! Load ls020_fb module first."
    exit 1
fi

# Проверить информацию о framebuffer
echo "Framebuffer info:"
cat /sys/class/graphics/fb0/virtual_size
echo "Color depth: $(cat /sys/class/graphics/fb0/bits_per_pixel) bits"
echo ""

# Установить переменные окружения
export DISPLAY=:1
export FRAMEBUFFER=/dev/fb0

# Остановить текущий X сервер (если запущен)
sudo systemctl stop lightdm 2>/dev/null || true
sudo systemctl stop gdm3 2>/dev/null || true
sudo killall Xorg 2>/dev/null || true

echo "Starting X server on display :1..."

# Запустить X сервер
sudo X :1 -config /home/orangepi/ls020_driver/xorg-ls020.conf -verbose 7 &

# Дождаться запуска X сервера
sleep 3

# Проверить что X сервер запустился
if pgrep -f "X :1" > /dev/null; then
    echo "✅ X server started successfully!"
    echo "To test, run: DISPLAY=:1 xterm &"
    echo "To stop: sudo killall Xorg"
else
    echo "❌ Failed to start X server. Check /var/log/Xorg.1.log"
fi