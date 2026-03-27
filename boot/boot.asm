; KumOS Multiboot entry point
; GRUB will load us, set up CPU, jump here

MBOOT_PAGE_ALIGN    equ 1<<0
MBOOT_MEM_INFO      equ 1<<1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top  ; set up stack
    push ebx            ; multiboot info struct pointer
    push eax            ; multiboot magic

    call kernel_main    ; jump to C kernel

    ; Should never return, but just in case:
.hang:
    cli
    hlt
    jmp .hang
