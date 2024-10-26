# Makefile for Zephyr OS project

# Variables
ASM = nasm
GCC = gcc
QEMU = qemu-system-i386
GRUB = grub-mkrescue
ISO_NAME = iso/zephyros.iso
BUILD_DIR = build_system/bin
ISO_DIR = iso

# Source files
BOOTLOADER_SRC = bootloader.asm
KERNEL_SRC = $(wildcard kernel/*.c)  # All C files in kernel directory
KERNEL_OBJ = $(KERNEL_SRC:.c=.o)

# Targets
.PHONY: all clean run

# Default target
all: $(BUILD_DIR)/boot.bin kernel/kernel.bin $(ISO_NAME)

# Rule to assemble the bootloader
$(BUILD_DIR)/boot.bin: $(BOOTLOADER_SRC)
	@mkdir -p $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

# Rule to compile the kernel
kernel/kernel.bin: $(KERNEL_OBJ)
	@mkdir -p kernel
	$(GCC) -m32 -o $@ $^

# Rule to create bootable ISO
$(ISO_NAME): $(BUILD_DIR)/boot.bin kernel/kernel.bin
	@mkdir -p $(ISO_DIR)/boot/grub
	echo "Creating GRUB configuration..."
	cat > $(ISO_DIR)/boot/grub/grub.cfg << EOF
set timeout=0
set default=0

menuentry "Zephyr OS" {
    multiboot /boot/kernel.bin
    boot
}
EOF
	cp $< $(ISO_DIR)/boot/
	cp kernel/kernel.bin $(ISO_DIR)/boot/
	$(GRUB) -o $@ $(ISO_DIR)

# Clean up build files
clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) kernel/*.o kernel/kernel.bin $(ISO_NAME)

# Run the ISO with QEMU
run: $(ISO_NAME)
	$(QEMU) -cdrom $(ISO_NAME)
