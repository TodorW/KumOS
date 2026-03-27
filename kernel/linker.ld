/* KumOS kernel linker script */
ENTRY(_start)

SECTIONS {
    . = 1M;     /* Load at 1 MB (standard for x86 kernels) */

    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        kernel_end = .;
    }
}
