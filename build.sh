#!/bin/bash

# KumOS Build Script

echo "================================"
echo "Building KumOS v1.0"
echo "================================"
echo ""

# Check for required tools
echo "[1/5] Checking dependencies..."
command -v nasm >/dev/null 2>&1 || { echo "Error: nasm not found. Install with: sudo apt-get install nasm"; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo "Error: gcc not found. Install with: sudo apt-get install gcc"; exit 1; }
command -v ld >/dev/null 2>&1 || { echo "Error: ld not found."; exit 1; }
echo "   ✓ All dependencies found"
echo ""

# Clean previous build
echo "[2/5] Cleaning previous build..."
make clean 2>/dev/null
echo "   ✓ Clean complete"
echo ""

# Build OS
echo "[3/5] Building bootloader and kernel..."
make all
if [ $? -ne 0 ]; then
    echo "   ✗ Build failed!"
    exit 1
fi
echo "   ✓ Build successful"
echo ""

# Check if image was created
echo "[4/5] Verifying OS image..."
if [ ! -f kumos.img ]; then
    echo "   ✗ OS image not created!"
    exit 1
fi
SIZE=$(stat -f%z "kumos.img" 2>/dev/null || stat -c%s "kumos.img" 2>/dev/null)
echo "   ✓ OS image created ($(($SIZE / 1024)) KB)"
echo ""

# Check for QEMU
echo "[5/5] Checking for QEMU..."
if command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "   ✓ QEMU found"
    echo ""
    echo "================================"
    echo "Build Complete!"
    echo "================================"
    echo ""
    echo "To run KumOS:"
    echo "  make run              # Run in QEMU"
    echo "  qemu-system-i386 -fda kumos.img"
    echo ""
    echo "To boot on real hardware:"
    echo "  dd if=kumos.img of=/dev/sdX  # Write to USB drive"
    echo ""
else
    echo "   ! QEMU not found (optional)"
    echo "   Install with: sudo apt-get install qemu-system-x86"
    echo ""
    echo "================================"
    echo "Build Complete!"
    echo "================================"
    echo ""
    echo "QEMU not available. To test:"
    echo "  1. Install QEMU: sudo apt-get install qemu-system-x86"
    echo "  2. Run: make run"
    echo "Or write to USB and boot on real hardware."
    echo ""
fi
