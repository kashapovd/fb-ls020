#!/bin/bash

echo "=== LS020 Performance Test ==="

# Test 1: SPI frequency
echo "1. Current SPI frequency:"
cat /sys/class/spi_master/spi1/device/spi1.0/max_speed_hz 2>/dev/null || echo "Not found"

# Test 2: Framebuffer write speed
echo "2. Testing framebuffer write speed..."
time dd if=/dev/zero of=/dev/fb0 bs=46464 count=1 2>/dev/null
echo ""

# Test 3: Color fill test
echo "3. Testing color changes..."
echo "Red..."
time (dd if=/dev/zero bs=46464 count=1 2>/dev/null | tr '\000' '\370' > /dev/fb0)
sleep 1
echo "Green..."  
time (dd if=/dev/zero bs=46464 count=1 2>/dev/null | tr '\000' '\016' > /dev/fb0)
sleep 1
echo "Blue..."
time (dd if=/dev/zero bs=46464 count=1 2>/dev/null | tr '\000' '\037' > /dev/fb0)
sleep 1

# Test 4: Clear screen
echo "4. Clearing screen..."
time dd if=/dev/zero of=/dev/fb0 bs=46464 count=1 2>/dev/null

echo "=== Test Complete ==="