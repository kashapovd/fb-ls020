#!/bin/bash

# Простые тесты X11 приложений для крошечного экрана

export DISPLAY=:1

echo "=== LS020 X11 Application Tests ==="

echo "1. Testing xterm (terminal) with default font..."
xterm -geometry 22x11 -bg black -fg green &
sleep 2

echo "2. Testing simple X11 tools..."
# Простые встроенные приложения
xlogo -geometry 50x50+0+0 &
sleep 1

echo "3. Testing xev (event monitor)..."
xev -geometry 100x50+50+50 &
sleep 2

echo "4. Testing bitmap editor..."
bitmap /tmp/test.xbm 50x50 &
sleep 2

echo "Applications started. Check your LS020 display!"
echo "Press Enter to close all applications..."
read

echo "Closing applications..."
killall xterm xlogo xev bitmap 2>/dev/null

echo "All test applications closed."