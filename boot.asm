; KumOS Bootloader
; This is a simple bootloader that loads our kernel

[BITS 16]           ; We start in 16-bit real mode
[ORG 0x7C00]        ; BIOS loads bootloader at this address

start:
    ; Set up segments
    cli             ; Disable interrupts
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00  ; Set stack pointer
    sti             ; Enable interrupts

    ; Print boot message
    mov si, boot_msg
    call print_string

    ; Load kernel from disk
    ; Read sectors from disk to memory
    mov ah, 0x02    ; BIOS read sector function
    mov al, 20      ; Number of sectors to read (increased for kernel)
    mov ch, 0       ; Cylinder 0
    mov cl, 2       ; Start from sector 2 (sector 1 is bootloader)
    mov dh, 0       ; Head 0
    mov dl, 0x00    ; Drive number (0x00 = first floppy)
    mov bx, 0x0000  ; ES:BX = 0000:1000
    mov es, bx
    mov bx, 0x1000  ; Load kernel at 0x0000:0x1000
    int 0x13        ; Call BIOS disk interrupt
    
    jc disk_error   ; Jump if error

    ; Jump to kernel
    mov si, kernel_msg
    call print_string
    
    ; Far jump to kernel entry point
    jmp 0x0000:0x1000

disk_error:
    mov si, error_msg
    call print_string
    jmp $           ; Hang

; Print string function
print_string:
    mov ah, 0x0E    ; BIOS teletype function
.loop:
    lodsb           ; Load byte from SI into AL
    test al, al     ; Check if zero (end of string)
    jz .done
    int 0x10        ; Print character
    jmp .loop
.done:
    ret

boot_msg:    db 'KumOS Bootloader v1.0', 13, 10, 'Loading kernel...', 13, 10, 0
kernel_msg:  db 'Jumping to kernel...', 13, 10, 0
error_msg:   db 'Disk read error!', 13, 10, 0

; Boot signature
times 510-($-$$) db 0
dw 0xAA55           ; Boot signature
