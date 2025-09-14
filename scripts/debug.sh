#!/bin/bash

echo "=== LS020 Debug Script ==="
echo

echo "1. Checking if framebuffer device exists:"
ls -la /dev/fb*
echo

echo "2. Checking GPIO status:"
sudo cat /sys/kernel/debug/gpio | grep -E "(dc|reset)"
echo

echo "3. Checking SPI devices:"
ls -la /sys/class/spi_master/spi1/device/
echo

echo "4. Latest kernel messages:"
dmesg | tail -n 20
echo

echo "5. Testing basic framebuffer access:"
if [ -e /dev/fb0 ]; then
    echo "Framebuffer /dev/fb0 exists"
    sudo dd if=/dev/zero of=/dev/fb0 bs=1024 count=46 2>/dev/null && echo "Successfully wrote to framebuffer"
else
    echo "Framebuffer /dev/fb0 does not exist"
fi
echo

echo "6. Hardware checks:"
echo "- Make sure display power (3.3V) is connected"
echo "- Check if backlight (9-12V) is connected and enabled"
echo "- Verify all SPI and GPIO connections"
echo "- Check if display controller initialization is working"