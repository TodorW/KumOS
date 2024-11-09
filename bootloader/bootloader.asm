[org 0x7C00]

KERNEL_SECTOR = 2
SECONDARY_KERNEL_SECTOR = 3
CONFIG_SECTOR = 4
SECTOR_SIZE = 512
ANIMATION_DELAY = 0x3FFFF
BOOT_TIMEOUT = 5

ERR_LOAD_PRIMARY = 1
ERR_LOAD_SECONDARY = 2
ERR_CHECKSUM_FAILED = 3
ERR_UNKNOWN = 255

start:
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    mov si, boot_msg
    call print_string
    call get_time
    mov [boot_start_time], ax
    call load_boot_config
    mov bx, 0x1000
    call load_kernel_with_animation
    mov si, success_msg
    call print_string
    call get_time
    mov ax, [boot_start_time]
    sub ax, [boot_end_time]
    call print_boot_time
    call display_memory_map
    jmp 0x1000

load_boot_config:
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, CONFIG_SECTOR
    mov dh, 0
    mov bx, 0x2000
    mov dl, [boot_drive]
    int 0x13
    ret

load_kernel_with_animation:
    mov cx, 0
    call start_animation
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, KERNEL_SECTOR
    mov dh, 0
    mov bx, 0x1000
    mov dl, [boot_drive]
    int 0x13
    call stop_animation
    jc load_secondary_kernel
    call validate_kernel_checksum
    jc load_error
    ret

validate_kernel_checksum:
    xor ax, ax
    mov cx, SECTOR_SIZE
    mov si, 0x1000
.checksum_loop:
    add ax, [si]
    inc si
    loop .checksum_loop
    cmp ax, [expected_checksum]
    jne .checksum_failed
    ret

.checksum_failed:
    mov ax, ERR_CHECKSUM_FAILED
    jmp load_error

load_secondary_kernel:
    mov si, secondary_kernel_msg
    call print_string
    mov bx, 0x2000
    call load_kernel
    jc load_error
    ret

load_kernel:
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, KERNEL_SECTOR
    mov dh, 0
    mov bx, 0x1000
    mov dl, [boot_drive]
    int 0x13
    ret

load_error:
    mov ax, [error_code]
    call print_error_message
    jmp $

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

start_animation:
    mov si, animation_frames
.animation_loop:
    call print_string
    call delay
    inc cx
    cmp cx, 4
    jl .animation_loop
    xor cx, cx
    jmp .animation_loop

stop_animation:
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0C
    int 0x10
    ret

delay:
    mov cx, ANIMATION_DELAY
.delay_loop:
    loop .delay_loop
    ret

print_string:
    mov ah, 0x0E
.next_char:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next_char
.done:
    ret

get_time:
    mov ax, 0x1234
    ret

print_boot_time:
    mov si, boot_time_msg
    call print_string
    ret

display_memory_map:
    mov ax, 0xE820
    xor ebx, ebx
    xor ecx, ecx
.next_entry:
    int 0x15
    jc .done
    mov si, memory_map_entry_msg
    call print_string
    mov ax, bx
    call print_hex
    mov ax, cx
    call print_hex
    mov al, dl
    call print_hex
    add ebx, 20
    jmp .next_entry
.done:
    ret

print_hex:
    mov si, hex_msg
    call print_string
    ret

print_environment_variables:
    mov si, environment_variables_msg
    call print_string
    ret

boot_timeout:
    mov cx, BOOT_TIMEOUT
.timeout_loop:
    call delay
    loop .timeout_loop
    jmp load_error

boot_msg db 'Booting KumOS...', 0
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

animation_frames db '.  ', 0
animation_frames db '.. ', 0
animation_frames db '...', 0
animation_frames db '  ', 0

boot_drive db 0x80
boot_start_time dw 0
boot_end_time dw 0
error_code dw 0
expected_checksum dw 0xABCD

times 510 - ($ - $$) db 0
dw 0xAA55
