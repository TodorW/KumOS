# KumOS Makefile
CC      = gcc
ASM     = nasm
LD      = ld

GCC_INCLUDES := $(shell gcc -m32 -ffreestanding -print-file-name=include)

CFLAGS  = -m32 -std=c99 -ffreestanding -O2 -Wall -Wextra \
          -Iinclude -Idrivers -Ikernel -Ishell \
          -isystem $(GCC_INCLUDES) \
          -fno-stack-protector -fno-pic -fno-builtin \
          -nostdlib -nostdinc

LDFLAGS = -m elf_i386 -T kernel/linker.ld --oformat elf32-i386

OBJS    = boot/boot.o \
          kernel/kernel.o \
          kernel/klibc.o \
          kernel/memory.o \
          drivers/vga.o \
          drivers/keyboard.o \
          shell/shell.o

ISO     = kumos.iso
KERNEL  = iso/boot/kumos.kernel

.PHONY: all iso clean

all: iso

boot/boot.o: boot/boot.asm
	$(ASM) -f elf32 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

drivers/%.o: drivers/%.c
	$(CC) $(CFLAGS) -c $< -o $@

shell/%.o: shell/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

iso: $(KERNEL)
	grub-mkrescue -o $(ISO) iso 2>/dev/null || \
	grub2-mkrescue -o $(ISO) iso

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)

# Run in QEMU (if available)
run:
	qemu-system-i386 -cdrom $(ISO) -m 32M

run-kvm:
	qemu-system-i386 -cdrom $(ISO) -m 32M -enable-kvm
