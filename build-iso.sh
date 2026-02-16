#!/bin/bash
# KumOS ISO Builder Script

echo "======================================"
echo "  KumOS ISO Builder"
echo "======================================"
echo ""

# Check for required tools
command -v genisoimage >/dev/null 2>&1 || command -v mkisofs >/dev/null 2>&1 || {
    echo "Error: genisoimage or mkisofs not found!"
    echo "Install with: sudo dnf install genisoimage"
    exit 1
}

# Build KumOS first
echo "[1/4] Building KumOS..."
make clean
make all || {
    echo "Build failed!"
    exit 1
}

echo ""
echo "[2/4] Creating ISO directory structure..."
mkdir -p iso/boot/grub

# Copy kernel
cp kumos.img iso/boot/

# Create GRUB config
echo "[3/4] Creating GRUB configuration..."
cat > iso/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0

menuentry "KumOS" {
    linux16 /boot/kumos.img
}
EOF

# Create ISO
echo "[4/4] Creating ISO image..."
if command -v genisoimage >/dev/null 2>&1; then
    genisoimage -R -b boot/grub/stage2_eltorito -no-emul-boot \
        -boot-load-size 4 -boot-info-table -o kumos.iso iso/ 2>/dev/null || \
    genisoimage -R -b boot/kumos.img -no-emul-boot \
        -boot-load-size 4 -o kumos.iso iso/
else
    mkisofs -R -b boot/kumos.img -no-emul-boot \
        -boot-load-size 4 -o kumos.iso iso/
fi

# Clean up
rm -rf iso/

echo ""
echo "======================================"
echo "  ISO Created Successfully!"
echo "======================================"
echo ""
echo "File: kumos.iso"
echo "Size: $(du -h kumos.iso | cut -f1)"
echo ""
echo "To run:"
echo "  qemu-system-i386 -cdrom kumos.iso"
echo ""
echo "To burn to CD/DVD:"
echo "  brasero kumos.iso"
echo "  or"
echo "  wodim -v kumos.iso"
echo ""
