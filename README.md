KumOS
=====

A minimal x86 operating system written in C and NASM assembly.
Boots from a CD-ROM image via GRUB, runs in QEMU or on real hardware.

Currently at v1.9.


What it has
-----------

Kernel:
  - Multiboot-compliant bootloader (GRUB)
  - GDT, IDT, 8259A PIC
  - PIT timer at 100Hz with preemptive round-robin scheduler
  - Two-level paging, physical memory manager, demand paging, COW
  - PS/2 keyboard driver (IRQ1), PS/2 mouse driver (IRQ12)
  - ATA PIO disk driver, FAT12 filesystem (read/write/delete/format)
  - Virtual filesystem (VFS) with /mem /disk /dev /proc mounts
  - In-memory filesystem
  - Pipe subsystem (ring buffer, blocking read/write)
  - INT 0x80 syscall interface (44 syscalls)
  - ELF32 loader (ring-3 userspace processes)
  - Signal subsystem (SIGINT, SIGTERM, SIGKILL, SIGCHLD)
  - RTL8139 NIC driver, ARP, IP/UDP stack, TCP (connect/send/recv/close)
  - /proc virtual filesystem (meminfo, uptime, ps, net, date, version)
  - RTC (CMOS clock), serial UART at 115200 baud (COM1)
  - User account system (root/user/guest, password hashing)
  - VGA Mode 13h graphics subsystem

Userspace:
  - kush  -- ring-3 shell (ls, cat, cp, rm, cd, pwd, pipe support)
  - libc  -- single-header C library (printf, malloc, sprintf, qsort, ...)
  - hello, counter, cat, sysinfo -- demo ELF programs
  - ed    -- line editor
  - top   -- process/memory monitor
  - crond -- background task scheduler
  - http  -- HTTP GET client over TCP

Shell commands (kernel shell):
  help, clear, echo, uname, whoami, hostname, date, uptime, history
  ls, cat, touch, write, rm                    (memory FS)
  dls, dcat, dwrite, drm, dformat, dcp         (FAT12 disk)
  ps, meminfo, vmem, cpuinfo, irqinfo, serial, hexdump
  proc [file]     -- read /proc/meminfo, /proc/ps, /proc/uptime, ...
  ifconfig        -- NIC info
  ping            -- UDP to gateway (10.0.2.2:7)
  netrecv [port]  -- listen for UDP
  kill <pid> [sig]
  wait <pid>
  login / users / whoami
  exec <file.elf> -- load and run ELF from disk
  kush            -- launch userspace shell
  gui             -- VGA Mode 13h desktop (experimental)
  reboot / shutdown (requires kum)


Build
-----

Dependencies (Fedora):
  sudo dnf install nasm gcc make grub2-tools-extra xorriso qemu-system-x86

Dependencies (Ubuntu/Debian):
  sudo apt install nasm gcc-multilib make grub-pc-bin grub-common xorriso qemu-system-x86

Build and run:
  make iso
  make run

With networking:
  make run-net

With serial log in terminal:
  make run-serial

Manual QEMU:
  qemu-system-x86_64 -enable-kvm -boot order=d \
      -cdrom kumos.iso -hda disk.img -m 128M -vga std -no-reboot

With networking:
  qemu-system-x86_64 -enable-kvm -boot order=d \
      -cdrom kumos.iso -hda disk.img -m 128M -vga std -nic user -no-reboot


Source layout
-------------

  boot/          NASM bootloader and ASM stubs
  src/           Kernel C sources
  user/          Userspace C programs and libc header
  iso/           ISO build directory (generated)
  linker.ld      Kernel linker script
  grub.cfg       GRUB menu config
  Makefile


Kernel source files
-------------------

  ata.c/h        ATA PIO disk driver
  elf.c/h        ELF32 loader
  fat12.c/h      FAT12 filesystem
  fs.c/h         In-memory filesystem
  gdt.c/h        Global Descriptor Table
  gui.c/h        VGA Mode 13h graphics
  idt.c/h        Interrupt Descriptor Table + 8259A PIC
  keyboard.c/h   PS/2 keyboard (IRQ1)
  kmalloc.c/h    Bump heap allocator
  kstring.c/h    String library
  mouse.c/h      PS/2 mouse (IRQ12)
  net.c/h        RTL8139 driver, ARP, IP, UDP, TCP
  paging.c/h     PMM, two-level paging, demand paging, COW
  pipe.c/h       Kernel pipe ring buffer
  procfs.c/h     /proc virtual filesystem
  process.c/h    Process table
  rtc.c/h        CMOS real-time clock
  sched.c/h      Preemptive round-robin scheduler
  serial.c/h     UART serial driver (COM1)
  signal.c/h     Signal subsystem
  syscall.c/h    INT 0x80 syscall interface
  timer.c/h      PIT 100Hz timer
  userspace.c/h  Ring-3 process spawner
  users.c/h      User account system
  vfs.c/h        Virtual filesystem layer
  vga.c/h        VGA text mode driver
  kernel.c       Kernel main, shell, boot sequence


Notes
-----

- Kernel loads at 0x100000 (1MB mark)
- User processes load at 0x400000
- Static heap at 0x200000 (512KB)
- Dynamic demand-paged heap starts at 0x280000
- FAT12 disk image: 1.44MB, standard floppy geometry
- Serial output at 115200 8N1 on COM1
- No comments in source code (intentional)
