
---

# KumOS

**KumOS** is a lightweight, modular operating system designed and implemented in C. Its focus is on simplicity, efficiency, and scalability. KumOS is built from the ground up, aiming to provide a minimal but flexible foundation for operating system design, kernel development, and system-level programming. It supports basic multitasking, memory management, process scheduling, and file system operations, all of which are essential for modern operating systems.

<div align="center">
  <img src="KumOS.png" alt="KumOS Logo" width="300"/>
</div>

---

## Table of Contents

- [Project Overview](#project-overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Technical Details](#technical-details)
- [Development Tools](#development-tools)
- [How to Contribute](#how-to-contribute)
- [License](#license)

---

## Project Overview

KumOS is intended for developers interested in learning or contributing to low-level system software. It is currently a work in progress, with core components like the kernel and shell being developed alongside memory management, process handling, and interrupt systems.

The aim of KumOS is to serve as both an educational project and a foundation for future expansion. It offers a platform where developers can experiment with various kernel features, file systems, and even drivers, while keeping the system modular and lightweight.

### Why "Kum"?
The name **Kum** reflects the project's philosophy: **reliable, efficient and simple**, akin to a gentle breeze. This OS is designed to be minimal yet powerful, much like the wind that’s swift but subtle.

---

## Features

### Core Components
- **Monolithic Kernel**: KumOS uses a monolithic kernel, where core services such as memory management, process scheduling, and hardware I/O are handled in a unified space.
- **Basic Shell**: A command-line interface shell for user interaction, supporting essential commands (`ls`, `cd`, `pwd`, etc.).
- **Memory Management**: Virtual memory management with support for paging and dynamic memory allocation.
- **Process Management**: Basic process scheduling, multitasking, and context switching.
- **File System**: Integration with a minimal file system for data storage and retrieval.
- **Interrupt Handling**: Support for hardware and software interrupts via an Interrupt Descriptor Table (IDT).
- **System Calls**: Provides an API for user-space programs to interact with kernel functions.

### Planned Features
- **Advanced File System Support**: Extensions to support popular file systems like FAT or ext2/3.
- **Networking**: Initial support for networking protocols, including a basic TCP/IP stack.
- **Graphical User Interface (GUI)**: A simple windowing system for a more user-friendly interface (future goal).

---

## Project Structure

The source code for KumOS is organized as follows:

```
/KumOS
├── /bootloader                # Bootloader code (e.g., GRUB config, or custom bootloader)
│   ├── bootloader.asm         # Assembly code for booting the OS
│   ├── boot.c                 # C code for initializing the boot process
│   ├── disk_io.c              # Functions for reading from disk
│   ├── elf_loader.c           # Code to load the kernel from an ELF file
│   └── gdt.c                  # Global Descriptor Table setup
│
├── /kernel                    # Core kernel code (scheduler, memory, system calls, etc.)
│   ├── kernel.c               # Main kernel entry point
│   ├── scheduler.c            # Implementation of scheduling algorithms
│   ├── interrupts.c           # Interrupt handling routines
│   ├── syscalls.c             # System call implementations
│   ├── process.c              # Process management functions
│   └── memory.c               # Basic memory management functions
│
├── /memory_management          # Memory management functionalities
│   ├── paging.c                # Implementation of paging and virtual memory
│   ├── heap.c                  # Dynamic memory allocation (malloc/free)
│   ├── memory_map.c            # Memory mapping functions
│   └── bitmap.c                # Bitmap for tracking free memory blocks
│
├── /process_management         # Process management functionalities
│   ├── process.c               # Process creation, termination, and management
│   ├── context_switch.c         # Context switching routines
│   ├── ipc.c                   # Inter-Process Communication mechanisms (e.g., pipes, message queues)
│   └── scheduler.c             # Scheduling algorithms (e.g., Round Robin, Priority)
│
├── /fs                        # File system implementations
│   ├── fs.c                   # File system interface and management
│   ├── fat.c                  # Implementation of FAT file system
│   ├── ext4.c                 # Implementation of ext4 file system
│   ├── file_ops.c             # File operations (open, read, write, close)
│   └── directory.c            # Directory management functions
│
├── /drivers                   # Device drivers for keyboard, disk, etc.
│   ├── keyboard.c             # Keyboard driver implementation
│   ├── mouse.c                # Mouse driver implementation
│   ├── disk.c                 # Disk driver for reading/writing to storage
│   ├── network.c              # Network device driver
│   └── serial.c               # Serial port driver for console output
│
├── /shell                     # Shell and user interaction layer
│   ├── shell.c                # Main shell program
│   ├── commands.c             # Implementation of shell commands (e.g., ls, cd)
│   └── input.c                # Handling user input and command parsing
│
├── /lib                       # Basic libraries (e.g., libc implementation)
│   ├── libc.c                 # Implementation of standard C library functions
│   ├── string.c               # String manipulation functions
│   ├── math.c                 # Basic math functions
│   └── utils.c                # Utility functions for kernel and userland
│
├── /userland                  # User-space applications and programs
│   ├── init.c                 # Initial userland program that starts the shell
│   ├── editor.c               # Simple text editor application
│   ├── file_manager.c         # File management utility
│   └── user_programs/         # Directory for additional userland applications
│
├── /docs                      # Documentation for the OS, API, and internal structures
│   ├── README.md              # Overview of the OS and its features
│   ├── API.md                 # Documentation for system calls and APIs
│   ├── USER_GUIDE.md          # User manual for operating the OS
│   └── DEVELOPER_GUIDE.md     # Developer documentation for contributing to the OS
│
├── /build_system              # Build system for compiling the OS
│   ├── Makefile               # Main build script for compiling the OS
│   ├── build.sh               # Shell script to automate the build process
│   └── config.mk              # Configuration settings for the build system
│
├── /cross_compiler_toolchain  # Cross-compiler toolchain setup
│   ├── toolchain_setup.sh     # Script to set up the cross-compiler
│   ├── gcc_configure.sh      # Configuration script for GCC
│   └── binutils_configure.sh # Configuration script for Binutils
│
├── /testing_debugging_tools   # Testing and debugging tools
│   ├── test_kernel.c          # Unit tests for kernel functionalities
│   ├── test_userland.c        # Unit tests for userland applications
│   ├── debug.c               # Debugging utilities and logging functions
│   └── gdb_setup.sh          # Script to set up GDB for debugging
│
├── /networking               # Networking stack implementation
│   ├── network.c             # Networking stack implementation (TCP/IP)
│   ├── socket.c              # Socket API implementation for userland
│   ├── dns.c                 # DNS query functions
│   └── ping.c                # Implementation of the ping utility
│
└── /gui                       # Graphical User Interface components
    # (To be implemented)
```

---

## Getting Started

### Prerequisites
To compile and run KumOS, you will need the following tools:
- **Cross Compiler**: GCC or Clang for cross-compiling to your target architecture (e.g., x86).
- **QEMU**: An emulator for running and testing the OS.
- **GRUB**: (Optional) A bootloader for loading your kernel.

### Building KumOS

1. **Clone the repository**:
   ```bash
   git clone https://github.com/yourusername/KumOS.git
   cd KumOS
   ```

2. **Configure the cross-compiler** (if not already set up):
   ```bash
   export PREFIX="/usr/local/i386elfgcc"
   export TARGET=i386-elf
   export PATH="$PREFIX/bin:$PATH"
   ```

3. **Build the kernel**:
   ```bash
   make
   ```

4. **Run KumOS on QEMU**:
   ```bash
   qemu-system-i386 -kernel path/to/Kumos-kernel
   ```

### Running KumOS
Once the system boots up, you can interact with the **Kum Shell**. Enter basic commands like `ls`, `cd`, and `pwd` to explore the system's directory structure, manage files, or start processes.

---

## Technical Details

### Memory Management
- **Paging**: KumOS implements basic virtual memory management using paging.
- **Dynamic Memory Allocation**: Memory is allocated dynamically through a `malloc()`-like interface in kernel-space.

### Process Management
- **Multitasking**: The kernel supports cooperative multitasking with basic process switching.
- **Scheduler**: A round-robin scheduler is implemented for simplicity, with plans to improve the algorithm.

### Interrupt Handling
- **Interrupt Descriptor Table (IDT)**: The kernel configures the IDT for handling hardware interrupts (e.g., keyboard input, timer).
- **Exception Handling**: Basic exception handling for CPU-generated errors like page faults or division by zero.

### File System
- **Basic File System**: KumOS comes with a minimal file system implementation that supports reading and writing files, with plans to add support for more advanced file systems like FAT and ext2.

---

## Development Tools

To aid development, KumOS leverages the following tools:
- **QEMU**: For system emulation and testing without requiring real hardware.
- **GDB**: For debugging kernel-level code using QEMU’s debugging integration.
- **Make/CMake**: For automating the build process and compiling the kernel, shell, and userland programs.

---

## How to Contribute

Contributions to KumOS are welcome! Whether you want to add new features, fix bugs, or improve documentation, your input will help this project grow.

To contribute:
1. Fork the repository.
2. Create a new branch for your feature/bugfix.
3. Submit a pull request with a detailed description of your changes.

Please make sure to follow the project's coding style and submit tests where applicable.

---

## License

KumOS is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

---

Thank you for your interest in KumOS! Feel free to open issues or discussions for any questions or suggestions.

---

*Rights and ownership of this software are exclusevily related to TodorW, not other contributors of it.
