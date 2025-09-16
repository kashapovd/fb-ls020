#!/bin/bash
export FRAMEBUFFER=fb0

echo "=== Starting Xorg on LS020 Display ==="

if [ ! -c /dev/$FRAMEBUFFER ]; then
    echo "ERROR: /dev/$FRAMEBUFFER not found! Load ls020_fb module first."
    exit 1
fi

echo "Framebuffer info:"
cat /sys/class/graphics/$FRAMEBUFFER/virtual_size 2>/dev/null || echo "Cannot read virtual_size"
echo "Color depth: $(cat /sys/class/graphics/$FRAMEBUFFER/bits_per_pixel 2>/dev/null || echo 'unknown') bits"
echo ""

export DISPLAY=:1
export XDG_RUNTIME_DIR=/tmp/runtime-$USER
export XDG_SESSION_TYPE=x11

mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR


sudo killall Xorg 2>/dev/null || true

echo "Starting X server on display :1..."
echo "Using XDG_RUNTIME_DIR: $XDG_RUNTIME_DIR"

# Запустить X сервер с исправленной конфигурацией
sudo X :1 -config xorg-ls020.conf -verbose 7 &

# Дождаться запуска X сервера
sleep 3

# Проверить что X сервер запустился
if pgrep -f "X :1" > /dev/null; then
    echo "✅ X server started successfully!"
    echo "Environment variables set:"
    echo "  DISPLAY=$DISPLAY"
    echo "  XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
    echo ""
    echo "To test applications:"
    echo "  DISPLAY=:1 XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR xterm &"
    echo "  DISPLAY=:1 XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR xclock &"
    echo ""
    echo "To stop: sudo killall Xorg"
else
    echo "❌ Failed to start X server. Check /var/log/Xorg.1.log"
    echo "Recent X server errors:"
    sudo tail -20 /var/log/Xorg.1.log 2>/dev/null || echo "No log file found"
fi