; bootloader.asm
; A simple bootloader for loading a ZephyrOS kernel with verbose boot messages, custom boot animations,
; fallback mechanism, boot time measurement, memory map display, error codes, environment variable support,
; boot timeout features, checksum validation, and boot configuration file support.

[org 0x7C00]        ; BIOS loads the bootloader at memory address 0x7C00

; Define some constants
KERNEL_SECTOR = 2            ; The sector where the primary kernel is located
SECONDARY_KERNEL_SECTOR = 3  ; The sector where the secondary kernel is located
CONFIG_SECTOR = 4            ; The sector where the boot configuration file is located
SECTOR_SIZE = 512             ; Size of a sector in bytes
ANIMATION_DELAY = 0x3FFFF     ; Delay for animation (tweak as needed)
BOOT_TIMEOUT = 5              ; Boot timeout in seconds

; Error codes
ERR_LOAD_PRIMARY = 1
ERR_LOAD_SECONDARY = 2
ERR_CHECKSUM_FAILED = 3
ERR_UNKNOWN = 255

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

    ; Load boot configuration
    call load_boot_config

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

    ; Display memory map
    call display_memory_map

    ; Jump to the kernel entry point
    jmp 0x1000                  ; Jump to the kernel

; Function to load the boot configuration
load_boot_config:
    mov ah, 0x02                ; BIOS function to read sectors
    mov al, 1                   ; Read 1 sector
    mov ch, 0                   ; Cylinder 0
    mov cl, CONFIG_SECTOR       ; Sector number
    mov dh, 0                   ; Head 0
    mov bx, 0x2000              ; Buffer to store the configuration
    mov dl, [boot_drive]        ; Read from the boot drive
    int 0x13                    ; Call BIOS interrupt
    ret

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

    ; Perform checksum validation
    call validate_kernel_checksum
    jc load_error                ; Jump to error if checksum failed

    ret

validate_kernel_checksum:
    ; Calculate checksum of the loaded kernel
    xor ax, ax                  ; Clear AX for checksum
    mov cx, SECTOR_SIZE         ; Number of bytes to read
    mov si, 0x1000              ; Start address of the kernel

.checksum_loop:
    add ax, [si]                ; Add byte at [si] to AX
    inc si                      ; Move to the next byte
    loop .checksum_loop

        ; Compare checksum (assuming we have a predefined checksum value)
    cmp ax, [expected_checksum]  ; Compare calculated checksum with expected
    jne .checksum_failed          ; If not equal, jump to checksum failure

    ret

.checksum_failed:
    mov ax, ERR_CHECKSUM_FAILED   ; Set error code for checksum failure
    jmp load_error                ; Jump to load error

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
    ; Print error message based on error code
    mov ax, [error_code]        ; Get the error code
    call print_error_message
    jmp $

; Function to print error messages based on error code
print_error_message:
    cmp ax, ERR_LOAD_PRIMARY
    je .load_primary_error
    cmp ax, ERR_LOAD_SECONDARY
    je .load_secondary_error
    cmp ax, ERR_CHECKSUM_FAILED
    je .checksum_error
    jmp .unknown_error

.load_primary_error:
    mov si, primary_error_msg
    call print_string
    ret

.load_secondary_error:
    mov si, secondary_error_msg
    call print_string
    ret

.checksum_error:
    mov si, checksum_error_msg
    call print_string
    ret

.unknown_error:
    mov si, unknown_error_msg
    call print_string
    ret

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
    mov ax, 0x1234             ; Dummy time value
    ret

; Function to print the boot time
print_boot_time:
    mov si, boot_time_msg
    call print_string
    ret

; Function to display the memory map
display_memory_map:
    mov ax, 0xE820            ; BIOS function to get memory map
    xor ebx, ebx              ; Start at the beginning of the memory map
    xor ecx, ecx              ; Set ECX to 0 (required by BIOS)
.next_entry:
    int 0x15                  ; Call BIOS interrupt
    jc .done                  ; If carry flag is set, we're done
    mov si, memory_map_entry_msg
    call print_string
        mov ax, bx                ; Print the base address
    call print_hex
    mov ax, cx                ; Print the length
    call print_hex
    mov al, dl                ; Print the type
    call print_hex
    ; Move to the next entry
    add ebx, 20               ; Each entry is 20 bytes
    jmp .next_entry
.done:
    ret

; Function to print a hexadecimal value
print_hex:
    ; Convert AX to a hexadecimal string and print it
    ; This is a placeholder; actual implementation may vary
    ; For demonstration, we will just print a static message
    mov si, hex_msg
    call print_string
    ret

; Environment variable support
print_environment_variables:
    mov si, environment_variables_msg
    call print_string
    ret

; Boot timeout feature
boot_timeout:
    ; Wait for BOOT_TIMEOUT seconds
    mov cx, BOOT_TIMEOUT
.timeout_loop:
    call delay
    loop .timeout_loop
    jmp load_error            ; If timeout, jump to load error

; Boot messages
boot_msg db 'Booting ZephyrOS...', 0
success_msg db 'Kernel loaded successfully!', 0
error_msg db 'Error loading kernel!', 0
secondary_kernel_msg db 'Loading secondary kernel...', 0
boot_time_msg db 'Boot time: ', 0
memory_map_entry_msg db 'Memory map entry: Base=0x', 0
hex_msg db ' (hex)', 0
primary_error_msg db 'Error loading primary kernel!', 0
secondary_error_msg db 'Error loading secondary kernel!', 0
checksum_error_msg db 'Checksum validation failed!', 0
unknown_error_msg db 'Unknown error!', 0
environment_variables_msg db 'Environment variables:', 0

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

; Error code
error_code dw 0

; Expected checksum value (for example, let's say the expected checksum is 0xABCD)
expected_checksum dw 0xABCD

; Fill the remaining space with zeros
times 510 - ($ - $$) db 0
dw 0xAA55              ; Boot signature