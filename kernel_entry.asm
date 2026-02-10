; KumOS Kernel Entry Point
; This sets up the environment and calls the main C kernel

[BITS 16]

section .text.kernel_entry
global kernel_entry
kernel_entry:
    ; Set up segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000  ; Set stack for kernel
    sti

    ; Switch to 32-bit protected mode
    call enable_a20
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Set PE bit in CR0
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to set CS
    jmp 0x08:protected_mode

; Enable A20 line for accessing memory above 1MB
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

[BITS 32]
protected_mode:
    ; Set up segment registers for protected mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000  ; Set stack pointer
    
    ; Call C kernel main function
    extern kernel_main
    call kernel_main
    
    ; Halt if kernel returns
    cli
    hlt

; Global Descriptor Table
gdt_start:
    ; Null descriptor
    dq 0x0

    ; Code segment descriptor
    dw 0xFFFF       ; Limit
    dw 0x0          ; Base (low)
    db 0x0          ; Base (middle)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit (high)
    db 0x0          ; Base (high)

    ; Data segment descriptor
    dw 0xFFFF       ; Limit
    dw 0x0          ; Base (low)
    db 0x0          ; Base (middle)
    db 10010010b    ; Access byte
    db 11001111b    ; Flags + Limit (high)
    db 0x0          ; Base (high)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                 ; Offset
