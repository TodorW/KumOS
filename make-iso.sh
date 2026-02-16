#!/bin/bash

echo "Building KumOS ISO (Floppy Emulation Mode)..."
echo ""

# Build KumOS
echo "[1/2] Building KumOS..."
make clean && make all || exit 1

# Create bootable ISO with floppy emulation
echo "[2/2] Creating ISO..."

# Method 1: Using xorriso (recommended)
if command -v xorriso >/dev/null 2>&1; then
    xorriso -as mkisofs \
        -o kumos.iso \
        -b kumos.img \
        -c boot.cat \
        -no-emul-boot \
        -boot-load-size 4 \
        .
    echo "✓ ISO created with xorriso"
# Method 2: Using genisoimage
elif command -v genisoimage >/dev/null 2>&1; then
    genisoimage -o kumos.iso \
        -b kumos.img \
        -c boot.cat \
        -no-emul-boot \
        -boot-load-size 4 \
        .
    echo "✓ ISO created with genisoimage"
# Method 3: Using mkisofs
elif command -v mkisofs >/dev/null 2>&1; then
    mkisofs -o kumos.iso \
        -b kumos.img \
        -c boot.cat \
        -no-emul-boot \
        -boot-load-size 4 \
        .
    echo "✓ ISO created with mkisofs"
else
    echo "Error: No ISO creation tool found!"
    echo "Install one of: xorriso, genisoimage, or mkisofs"
    echo ""
    echo "Fedora: sudo dnf install xorriso"
    echo "Ubuntu: sudo apt install xorriso"
    exit 1
fi

echo ""
echo "============================"
echo "ISO Created: kumos.iso"
echo "Size: $(du -h kumos.iso 2>/dev/null | cut -f1)"
echo "============================"
echo ""
echo "Run with:"
echo "  qemu-system-i386 -cdrom kumos.iso"
echo ""
echo "Or boot from real CD/DVD!"
echo ""
