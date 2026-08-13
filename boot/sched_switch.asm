
global switch_context
global user_entry_trampoline

section .note.GNU-stack noalloc noexec nowrite progbits

section .text

user_entry_trampoline:
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    iret

switch_context:

    mov eax, [esp+4]
    mov ecx, [esp+8]

    ; EFLAGS is now part of the saved context (popfd below), not a blanket
    ; sti. A blanket sti here re-enabled interrupts the instant *any* task
    ; got switched to, even one resuming mid-way through an isr128/irq
    ; handler's own hand-rolled epilogue (still cli'd, with its own later
    ; iret still pending) - a nested timer tick landing in that reopened
    ; window could preempt AGAIN before that epilogue finished, corrupting
    ; it. popfd instead restores each task's own last-saved IF state, so a
    ; task parked mid-epilogue resumes exactly as cli'd as it was.
    pushfd
    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp

    mov esp, ecx

    pop edi
    pop esi
    pop ebx
    pop ebp
    popfd

    ret
