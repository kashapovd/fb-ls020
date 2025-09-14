#!/bin/bash

export DISPLAY=:1

echo "=== LS020 Simple Demo ==="

# Запустить оконный менеджер
echo "Starting window manager..."
twm &
sleep 1

# Запустить терминал
echo "Starting terminal..."
xterm -geometry 22x11 -bg black -fg green -title "LS020 Terminal" &
sleep 2

# Показать информацию о дисплее
echo "Display info:"
DISPLAY=:1 xwininfo -root | grep -E "(Width|Height|Depth)"

echo ""
echo "✅ LS020 Xorg demo is running!"
echo "You should see a green terminal on your tiny display."
echo ""
echo "Commands to try:"
echo "  DISPLAY=:1 xlogo &          # Show X logo"
echo "  DISPLAY=:1 xterm &          # Another terminal"
echo "  DISPLAY=:1 xev &            # Event monitor"
echo ""
echo "Press Enter to stop demo..."
read

echo "Stopping demo..."
killall twm xterm xlogo xev 2>/dev/null

echo "Demo stopped."