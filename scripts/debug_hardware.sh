#!/bin/bash
# LS020 Hardware Debug Script

echo "=== LS020 Hardware Debug ==="
echo

echo "1. Checking GPIO states:"
sudo cat /sys/kernel/debug/gpio | grep -E "(228|270)"
echo

echo "2. Checking SPI bus:"
ls -la /dev/spi*
echo

echo "3. Checking framebuffer:"
ls -la /dev/fb*
echo

echo "4. Manual GPIO test - toggling reset pin:"
echo "Toggling PI14 (reset) 5 times..."
for i in {1..5}; do
    echo "Toggle $i"
    echo 270 > /sys/class/gpio/export 2>/dev/null
    echo out > /sys/class/gpio/gpio270/direction 2>/dev/null
    echo 0 > /sys/class/gpio/gpio270/value
    sleep 0.5
    echo 1 > /sys/class/gpio/gpio270/value
    sleep 0.5
done
echo 270 > /sys/class/gpio/unexport 2>/dev/null
echo

echo "5. Manual SPI test:"
echo "Testing basic SPI communication..."
# Simple command to display
echo -ne '\xEF\x90' > /dev/spidev1.0 2>/dev/null || echo "No /dev/spidev1.0 available (normal with your driver)"
echo

echo "6. Display power check:"
echo "⚠️  IMPORTANT: Check your backlight power supply!"
echo "   - Pin 2 (LED+) should have 9-12V"
echo "   - Pin 1 (LED-) should be connected to GND"
echo "   - Without backlight power, display will be completely black"
echo

echo "7. Try filling framebuffer with white:"
if [ -e /dev/fb0 ]; then
    echo "Filling /dev/fb0 with white color..."
    dd if=/dev/zero bs=46464 count=1 2>/dev/null | tr '\000' '\377' > /dev/fb0
    echo "If display is working, you should see white screen now"
else
    echo "No /dev/fb0 found"
fi