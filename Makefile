GCC_INC := $(shell find /usr/lib/gcc -name "stddef.h" 2>/dev/null | head -1 | xargs dirname)
CC       = gcc
CFLAGS   = -m32 -ffreestanding -O2 -Wall -fno-stack-protector -fno-builtin \
           -nostdlib -nostdinc -I$(GCC_INC) -Isrc
ASM      = nasm
ASMFLAGS = -f elf32
LD       = ld
LDFLAGS  = -m elf_i386 -T linker.ld
UFLAGS   = -m32 -nostdlib -nostartfiles -static -O2 -fno-stack-protector \
           -fno-builtin -Wl,--build-id=none -Wl,-z,norelro -Iuser
GRUB_MKR = $(shell command -v grub2-mkrescue 2>/dev/null || command -v grub-mkrescue 2>/dev/null)

KERN_OBJS = \
    boot/boot.o boot/gdt_flush.o boot/isr_stubs.o boot/sched_switch.o \
    src/kstring.o src/vga.o src/keyboard.o src/kmalloc.o \
    src/process.o src/fs.o src/gdt.o src/idt.o \
    src/timer.o src/sched.o src/paging.o \
    src/ata.o src/fat12.o src/pipe.o src/vfs.o \
    src/signal.o src/net.o src/procfs.o src/users.o \
    src/dns.o src/dmesg.o src/dhcp.o src/ext2.o src/swap.o \
    src/syscall.o src/userspace.o src/elf.o \
    src/serial.o src/rtc.o src/mouse.o src/gui.o src/kernel.o

USER_PROGS = \
    user/hello.elf user/counter.elf user/cat.elf user/sysinfo.elf \
    user/kush.elf user/ed.elf user/vi.elf user/top.elf \
    user/crond.elf user/http.elf user/grep.elf user/tar.elf \
    user/wc.elf user/sort.elf user/uniq.elf user/awk.elf

all: kumos.bin
boot/%.o: boot/%.asm
	@$(ASM) $(ASMFLAGS) $< -o $@
src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
kumos.bin: $(KERN_OBJS)
	@$(LD) $(LDFLAGS) -o $@ $(KERN_OBJS) 2>&1 | grep -v deprecated
	@echo "Kernel: $$(ls -lh $@ | awk '{print $$5}')"
user/%.elf: user/%.c
	@$(CC) $(UFLAGS) -Ttext=0x400000 -o $@ $<
	@strip $@
user-programs: $(USER_PROGS)
iso: kumos.bin
	@mkdir -p iso/boot/grub
	@cp kumos.bin iso/boot/kumos.bin && cp grub.cfg iso/boot/grub/grub.cfg
	@$(GRUB_MKR) -o kumos.iso iso/ 2>/dev/null
	@echo "ISO: $$(ls -lh kumos.iso | awk '{print $$5}')"
run: iso
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -hda disk.img -m 128M -vga std -no-reboot
run-net: iso
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -hda disk.img -m 128M -vga std -no-reboot \
	    -nic user
run-serial: iso
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -hda disk.img -m 128M -vga std -no-reboot \
	    -serial stdio
clean:
	@rm -f $(KERN_OBJS) kumos.bin kumos.iso iso/boot/kumos.bin user/*.elf
.PHONY: all iso run run-net run-serial clean user-programs
