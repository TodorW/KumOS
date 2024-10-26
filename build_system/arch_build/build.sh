#!/usr/bin/env bash 

# Automatically make the script executable
chmod +x $0

# Arch Linux Build

# Function to check if required commands are installed
check_package() {
    local cmds=("nasm" "grub" "make" "qemu-system-i386")
    local missing=()

    echo -ne "\nChecking required dependencies...\n"

    for cmd in "${cmds[@]}"; do
        if ! command -v $cmd &> /dev/null; then
            echo -ne "\n$cmd is not installed. Installing...\n"
            sudo pacman -S --noconfirm $cmd || { echo -ne "\nFailed to install $cmd. Please install it manually.\n"; exit 1; }
        else 
            echo -ne "\n$cmd: Package already installed.\n"
        fi 
    done

    echo -ne "\nAll dependencies are installed.\n"
}

# Run dependency check
check_package

# Define variables
ISO_NAME="iso/zephyros.iso"
BUILD_DIR="build_system/bin"
BOOT_DIR="bootloader"
ISO_DIR="iso"

# Prepare directories
echo -ne "\nSetting up build and ISO directories...\n"
mkdir -p $BUILD_DIR
mkdir -p $ISO_DIR/boot/grub
touch $ISO_DIR/boot/grub/grub.cfg
touch $ISO_DIR/boot/kernel.bin

# Assemble the bootloader
echo -ne "Assembling bootloader..."
nasm -f bin $BOOT_DIR/bootloader.asm -o $BUILD_DIR/boot.bin || { echo -ne "\nBootloader assembly failed\n"; exit 1; }

# Compile the kernel
echo -ne "Compiling kernel..."
cd kernel || { echo -ne "\nKernel directory not found\n"; exit 1; }
make || { echo -ne "\nKernel compilation failed\n"; exit 1; }
cd -

# Set up GRUB configuration
echo -ne "Creating GRUB configuration..."
cat > $ISO_DIR/boot/grub/grub.cfg << EOF
set timeout=0
set default=0

menuentry "ZephyrOS" {
    multiboot /boot/kernel.bin
    boot
}
EOF

# Copy binaries to ISO directory
echo -ne "\nCopying bootloader and kernel to ISO directory...\n"
cp $BUILD_DIR/boot.bin $ISO_DIR/boot/
cp kernel/kernel.bin $ISO_DIR/boot/

# Create the bootable ISO
echo -ne "\nCreating bootable ISO...\n"
grub-mkrescue -o $ISO_NAME $ISO_DIR || { echo -ne "\nISO creation failed\n"; exit 1; }

# Run the ISO with QEMU
echo -ne "\nStarting QEMU...\n"
qemu-system-i386 -cdrom $ISO_NAME