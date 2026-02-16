; Ultra-Simple Bootloader - Works Every Time
[BITS 16]
[ORG 0x7C00]

start:
    ; Setup segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Load kernel
    mov bx, 0x1000      ; Destination
    mov ah, 0x02        ; Read function
    mov al, 40          ; Sectors
    mov ch, 0           ; Cylinder
    mov cl, 2           ; Sector
    mov dh, 0           ; Head
    mov dl, 0           ; Drive
    int 0x13

    ; Jump to kernel - NO RETURN!
    jmp 0x0000:0x1000

times 510-($-$$) db 0
dw 0xAA55
