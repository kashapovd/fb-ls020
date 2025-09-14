#!/bin/bash

export DISPLAY=:1

echo "Starting minimal window manager for LS020..."

# Простой оконный менеджер для крошечного экрана
twm &

# Или можно использовать еще более простой
# ratpoison &
# dwm &

echo "Window manager started. Use test_x11_apps.sh to test applications."