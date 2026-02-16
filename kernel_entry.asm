; KumOS Kernel Entry Point
[BITS 16]

section .text
global kernel_entry

kernel_entry:
    ; We're now at 0x1000
    ; Clear screen first
    mov ax, 0x0003
    int 0x10
    
    ; Print startup message
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov si, startup_msg
    mov ah, 0x0F
.print_loop:
    lodsb
    test al, al
    jz .print_done
    stosw
    jmp .print_loop
.print_done:

    ; Setup segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    ; Enable A20
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to protected mode code
    jmp 0x08:protected_mode

startup_msg: db 'KumOS Starting...', 0

[BITS 32]
protected_mode:
    ; Setup protected mode segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Jump to C kernel
    extern kernel_main
    call kernel_main

    ; Halt if kernel returns
    cli
.hang:
    hlt
    jmp .hang

; GDT
align 4
gdt_start:
    ; Null descriptor
    dd 0x00000000
    dd 0x00000000

    ; Code segment (0x08)
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit 16-19
    db 0x00         ; Base 24-31

    ; Data segment (0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
