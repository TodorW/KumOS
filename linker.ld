/* KumOS Kernel Linker Script */
ENTRY(_start)

SECTIONS {
    . = 0x00100000;     /* Load at 1MB */

    .multiboot : {
        *(.multiboot)
    }

    .text ALIGN(4096) : {
        *(.text*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata*)
    }

    .data ALIGN(4096) : {
        *(.data*)
    }

    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss*)
    }
}
