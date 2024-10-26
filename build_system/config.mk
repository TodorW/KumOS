# config.mk - Configuration file for Zephyr OS

# Directories
SRC_DIR = src
BUILD_DIR = build_system/bin
ISO_DIR = iso
KERNEL_DIR = kernel

# Toolchain
ASM = nasm
GCC = gcc
QEMU = qemu-system-i386
GRUB = grub-mkrescue

# ISO Name
ISO_NAME = $(ISO_DIR)/zephyros.iso

# Compiler Flags
CFLAGS = -m32 -Wall -Wextra -O2

# Assembler Flags
ASMFLAGS = -f bin

# List of source files
KERNEL_SRC = $(wildcard $(KERNEL_DIR)/*.c)  # All C files in kernel directory
KERNEL_OBJ = $(KERNEL_SRC:.c=.o)

# Output files
BOOTLOADER_BIN = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(KERNEL_DIR)/kernel.bin

# Additional configuration (if needed)
GRUB_CFG = $(ISO_DIR)/boot/grub/grub.cfg
