# KumOS Makefile

# Compiler and tools
ASM = nasm
CC = gcc
LD = ld

# Flags
ASMFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -c
LDFLAGS = -m elf_i386 -T linker.ld

# Source files
BOOT_SRC = boot.asm
KERNEL_ENTRY_SRC = kernel_entry.asm
C_SOURCES = kernel.c terminal.c string.c keyboard.c shell.c timer.c
OBJS = kernel_entry.o kernel.o terminal.o string.o keyboard.o shell.o timer.o

# Output files
BOOTLOADER = boot.bin
KERNEL = kernel.bin
OS_IMAGE = kumos.img

.PHONY: all clean run

all: $(OS_IMAGE)

# Compile bootloader
$(BOOTLOADER): $(BOOT_SRC)
	$(ASM) -f bin $(BOOT_SRC) -o $(BOOTLOADER)

# Compile kernel entry
kernel_entry.o: $(KERNEL_ENTRY_SRC)
	$(ASM) $(ASMFLAGS) $(KERNEL_ENTRY_SRC) -o kernel_entry.o

# Compile C source files
%.o: %.c kernel.h
	$(CC) $(CFLAGS) $< -o $@

# Link kernel
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)
	@echo "Kernel size: $$(stat -c%s $(KERNEL) 2>/dev/null || stat -f%z $(KERNEL)) bytes"

# Create OS image
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL)
	@echo "Creating OS image..."
	# Create blank 1.44MB image
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	# Write bootloader to first sector
	dd if=$(BOOTLOADER) of=$(OS_IMAGE) bs=512 count=1 conv=notrunc 2>/dev/null
	# Write kernel starting at sector 2
	dd if=$(KERNEL) of=$(OS_IMAGE) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Bootloader size: $$(stat -c%s $(BOOTLOADER) 2>/dev/null || stat -f%z $(BOOTLOADER)) bytes (should be 512)"
	@echo "OS image size: $$(stat -c%s $(OS_IMAGE) 2>/dev/null || stat -f%z $(OS_IMAGE)) bytes"
	@echo "Build complete!"

# Run in QEMU
run: $(OS_IMAGE)
	qemu-system-i386 -fda $(OS_IMAGE)

# Run in QEMU without graphics (for testing)
run-nographic: $(OS_IMAGE)
	qemu-system-i386 -fda $(OS_IMAGE) -nographic

# Clean build files
clean:
	rm -f *.o *.bin $(OS_IMAGE)

# Build and run
build-run: all run
