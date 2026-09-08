; isr_asm.asm — ISR/IRQ 汇编桩 & GDT/IDT flush
; 每个 ISR/IRQ 入口: push 错误码/0 → push interrupt_no → jmp common

%macro ISR_NOERR 1
    global isr%1
    isr%1:
        push 0          ; dummy error code
        push %1         ; interrupt number
        jmp isr_common
%endmacro

%macro ISR_ERR 1
    global isr%1
    isr%1:
        push %1         ; interrupt number (error code already pushed by CPU)
        jmp isr_common
%endmacro

%macro IRQ 2
    global irq%1
    irq%1:
        push 0
        push %2         ; IRQ number + 32
        jmp irq_common
%endmacro

; ISR 0-31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQ 0-15
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern isr_handler
extern irq_handler

isr_common:
    pushad          ; 保存 eax,ecx,edx,ebx,esp,ebp,esi,edi
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; 传递 Registers* 指针
    call isr_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp, 8      ; 清理 error code + int_no
    iret

irq_common:
    pushad
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp, 8
    iret

; GDT flush
global gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]

    mov ax, 0x10    ; 数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush  ; 远跳转刷新 CS
.flush:
    ret

; IDT flush
global idt_flush
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret
