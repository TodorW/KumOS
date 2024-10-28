; bootloader.asm
; A simple bootloader for loading a ZephyrOS kernel

[org 0x7C00]        ; BIOS loads the bootloader at memory address 0x7C00

; Define some constants
KERNEL_SECTOR = 2  ; The sector where the kernel is located
SECTOR_SIZE = 512   ; Size of a sector in bytes

; Bootloader entry point
start:
    ; Set up the stack
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load the kernel from disk
    mov bx, 0x1000        ; Load kernel at 0x1000
    call load_kernel

    ; Jump to the kernel entry point
    jmp 0x1000            ; Jump to the kernel

; Function to load the kernel from disk
load_kernel:
    mov ah, 0x02         ; BIOS function to read sectors
    mov al, 1            ; Read 1 sector
    mov ch, 0            ; Cylinder 0
    mov cl, KERNEL_SECTOR ; Sector number
    mov dh, 0            ; Head 0
    mov bx, 0x1000       ; Buffer to store the kernel
    mov dl, [boot_drive] ; Read from the boot drive
    int 0x13             ; Call BIOS interrupt

    ; Check for errors
    jc load_error         ; Jump if carry flag is set (error)

    ret

load_error:
    ; Print an error message (a simple loop)
    mov si, error_msg
    call print_string
    hlt                   ; Halt the CPU

; Function to print a string
print_string:
    mov ah, 0x0E         ; BIOS teletype output function
.next_char:
    lodsb                ; Load byte at DS:SI into AL and increment SI
    cmp al, 0            ; Check for null terminator
    je .done             ; If null, done
    int 0x10             ; Print character in AL
    jmp .next_char
.done:
    ret

; Error message
error_msg db 'Error loading kernel!', 0

; Boot drive (assumed to be 0x80 for the first hard disk)
boot_drive db 0x80

; Fill the remaining space with zeros
times 510 - ($ - $$) db 0
dw 0xAA55              ; Boot signature