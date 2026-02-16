; Minimal Kernel
[BITS 16]
[ORG 0x1000]

start:
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    ; Print "KERNEL OK" directly to video memory
    mov ax, 0xB800
    mov es, ax
    xor di, di
    
    ; K
    mov byte [es:di], 'K'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; E
    mov byte [es:di], 'E'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; R
    mov byte [es:di], 'R'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; N
    mov byte [es:di], 'N'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; E
    mov byte [es:di], 'E'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; L
    mov byte [es:di], 'L'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; Space
    mov byte [es:di], ' '
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; O
    mov byte [es:di], 'O'
    mov byte [es:di+1], 0x0A
    add di, 2
    
    ; K
    mov byte [es:di], 'K'
    mov byte [es:di+1], 0x0A
    
    ; Infinite halt loop
    cli
hang:
    hlt
    jmp hang

times 10240-($-$$) db 0  ; Pad to 20 sectors
