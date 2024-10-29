; bootloader.asm
; A simple bootloader for loading a ZephyrOS kernel with verbose boot messages, custom boot animations,
; fallback mechanism, and boot time measurement.

[org 0x7C00]        ; BIOS loads the bootloader at memory address 0x7C00

; Define some constants
KERNEL_SECTOR = 2            ; The sector where the primary kernel is located
SECONDARY_KERNEL_SECTOR = 3  ; The sector where the secondary kernel is located
SECTOR_SIZE = 512             ; Size of a sector in bytes
ANIMATION_DELAY = 0x3FFFF     ; Delay for animation (tweak as needed)

; Bootloader entry point
start:
    ; Set up the stack
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print boot message
    mov si, boot_msg
    call print_string

    ; Start measuring boot time
    call get_time
    mov [boot_start_time], ax  ; Store the start time

    ; Attempt to load the primary kernel
    mov bx, 0x1000              ; Load primary kernel at 0x1000
    call load_kernel_with_animation

    ; Print success message
    mov si, success_msg
    call print_string

    ; Stop measuring boot time
    call get_time
    mov ax, [boot_start_time]   ; Load the start time
    sub ax, [boot_end_time]     ; Calculate boot time
    call print_boot_time

    ; Jump to the kernel entry point
    jmp 0x1000                  ; Jump to the kernel

; Function to load the kernel from disk with animation
load_kernel_with_animation:
    ; Start the animation
    mov cx, 0                   ; Animation frame counter
    call start_animation

    ; Load the kernel from disk
    mov ah, 0x02                ; BIOS function to read sectors
    mov al, 1                   ; Read 1 sector
    mov ch, 0                   ; Cylinder 0
    mov cl, KERNEL_SECTOR       ; Sector number
    mov dh, 0                   ; Head 0
    mov bx, 0x1000              ; Buffer to store the kernel
    mov dl, [boot_drive]        ; Read from the boot drive
    int 0x13                    ; Call BIOS interrupt

    ; Stop the animation
    call stop_animation

    ; Check for errors
    jc load_secondary_kernel     ; Jump to load secondary kernel if error

    ret

load_secondary_kernel:
    ; Attempt to load the secondary kernel
    mov si, secondary_kernel_msg
    call print_string

    mov bx, 0x2000              ; Load secondary kernel at 0x2000
    call load_kernel

    ; Check for errors
    jc load_error                ; Jump if carry flag is set (error)

    ret

load_kernel:
    ; Load the kernel from disk
    mov ah, 0x02                ; BIOS function to read sectors
    mov al, 1                   ; Read 1 sector
    mov ch, 0                   ; Cylinder 0
    mov cl, KERNEL_SECTOR       ; Sector number
    mov dh, 0                   ; Head 0
    mov bx, 0x1000              ; Buffer to store the kernel
    mov dl, [boot_drive]        ; Read from the boot drive
    int 0x13                    ; Call BIOS interrupt

    ; Check for errors
    ret

load_error:
    mov si, error_msg
    call print_string
    jmp $

; Function to start the animation
start_animation:
    ; Print loading animation
    mov si, animation_frames
.animation_loop:
    ; Print the current animation frame
    call print_string
    ; Delay for a moment
    call delay
    ; Update frame counter
    inc cx
    cmp cx, 4                   ; We have 4 frames (0 to 3)
    jl .animation_loop

    ; Reset frame counter
    xor cx, cx
    jmp .animation_loop         ; Loop forever (until loading is done)

stop_animation:
    ; Clear the line after animation
    mov ah, 0x0E
    mov al, 0x0D               ; Carriage return
    int 0x10
    mov al, 0x0C               ; Clear line
    int 0x10
    ret

; Function to delay (simple loop)
delay:
    mov cx, ANIMATION_DELAY
.delay_loop:
    loop .delay_loop
    ret

; Function to print a string
print_string:
    mov ah, 0x0E               ; BIOS teletype output function
.next_char:
    lodsb                       ; Load byte at DS:SI into AL and increment SI
    cmp al, 0                  ; Check for null terminator
    je .done                    ; If null, done
    int 0x10                    ; Print character in AL
    jmp .next_char
.done:
    ret

; Function to get the current time (in ticks)
get_time:
    ; Read the current time from the timer
    ; This is a placeholder; actual implementation may vary
    ; For demonstration, we will just set AX to a dummy value
    mov ax, 0x1234             ; Dummy time value
    ret

; Function to print the boot time
print_boot_time:
    ; Convert AX to string and print it
    ; This is a placeholder; actual implementation may vary
    ; For demonstration, we will just print a static message
    mov si, boot_time_msg
    call print_string
    ret

; Boot messages
boot_msg db 'Booting ZephyrOS...', 0
success_msg db 'Kernel loaded successfully!', 0
error_msg db 'Error loading kernel!', 0
secondary_kernel_msg db 'Loading secondary kernel...', 0
boot_time_msg db 'Boot time: ', 0

; Animation frames
animation_frames db '.  ', 0 ; Frame 0
animation_frames db '.. ', 0  ; Frame 1
animation_frames db '...', 0   ; Frame 2
animation_frames db '  ', 0     ; Frame 3 (clear line)

; Boot drive (assumed to be 0x80 for the first hard disk)
boot_drive db 0x80

; Boot start and end times
boot_start_time dw 0
boot_end_time dw 0

; Fill the remaining space with zeros
times 510 - ($ - $$) db 0
dw 0xAA55              ; Boot signature