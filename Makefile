GCC_INC := $(shell find /usr/lib/gcc -name "stddef.h" 2>/dev/null | head -1 | xargs dirname)
CC       = gcc
CFLAGS   = -m32 -g -ffreestanding -O2 -Wall -fno-stack-protector -fno-builtin \
           -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mgeneral-regs-only \
           -nostdlib -nostdinc -I$(GCC_INC) -Isrc
ASM      = nasm
ASMFLAGS = -f elf32
LD       = ld
LDFLAGS  = -m elf_i386 -T linker.ld
UFLAGS   = -m32 -nostdlib -nostartfiles -static -O2 -fno-stack-protector \
           -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mgeneral-regs-only \
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
    src/serial.o src/rtc.o src/mouse.o src/gui.o src/pkg.o src/kernel.o

USER_PROGS = \
    user/hello.elf user/counter.elf user/cat.elf user/sysinfo.elf \
    user/kush.elf user/ed.elf user/vi.elf user/top.elf \
    user/crond.elf user/http.elf user/grep.elf user/tar.elf \
    user/wc.elf user/sort.elf user/uniq.elf user/awk.elf

PKG_NAMES = hello counter cat sysinfo kush ed vi top crond http grep tar wc sort uniq awk
PKG_BLOBS = $(addprefix user/,$(addsuffix _blob.o,$(PKG_NAMES)))

all: kumos.bin
boot/%.o: boot/%.asm
	@$(ASM) $(ASMFLAGS) $< -o $@
src/%.o: src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
user/%_blob.o: user/%.elf
	@cd user && objcopy -I binary -O elf32-i386 -B i386 $*.elf $*_blob.o
kumos.bin: $(KERN_OBJS) $(PKG_BLOBS)
	@$(LD) $(LDFLAGS) -o $@ $(KERN_OBJS) $(PKG_BLOBS) 2>/tmp/kumos-ld-stderr.$$$$; ec=$$?; grep -v deprecated /tmp/kumos-ld-stderr.$$$$ 1>&2; rm -f /tmp/kumos-ld-stderr.$$$$; exit $$ec
	@echo "Kernel: $$(ls -lh $@ | awk '{print $$5}')"
user/%.elf: user/%.c
	@$(CC) $(UFLAGS) -Ttext=0x400000 -o $@ $<
	@strip $@
user-programs: $(USER_PROGS)
iso: kumos.bin
	@mkdir -p iso/boot/grub/themes/kumos
	@cp kumos.bin iso/boot/kumos.bin && cp grub.cfg iso/boot/grub/grub.cfg
	@cp -r grub-theme/* iso/boot/grub/themes/kumos/
	@$(GRUB_MKR) -o kumos.iso iso/ 2>/dev/null
	@echo "ISO: $$(ls -lh kumos.iso | awk '{print $$5}')"
ext2.img:
	@dd if=/dev/zero of=$@ bs=1M count=8 status=none
run: iso ext2.img
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -drive file=disk.img,format=raw,if=ide -drive file=ext2.img,format=raw,if=ide -m 128M -vga std -no-reboot
run-net: iso ext2.img
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -drive file=disk.img,format=raw,if=ide -drive file=ext2.img,format=raw,if=ide -m 128M -vga std -no-reboot \
	    -nic user
run-serial: iso ext2.img
	@qemu-system-x86_64 $$([ -r /dev/kvm ] && echo "-enable-kvm") \
	    -boot order=d -cdrom kumos.iso -drive file=disk.img,format=raw,if=ide -drive file=ext2.img,format=raw,if=ide -m 128M -vga std -no-reboot \
	    -serial stdio
test: iso ext2.img
	@./smoke_test.sh
clean:
	@rm -f $(KERN_OBJS) $(PKG_BLOBS) kumos.bin kumos.iso iso/boot/kumos.bin user/*.elf
.PHONY: all iso run run-net run-serial test clean user-programs
