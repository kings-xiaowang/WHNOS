; boot.asm — Multiboot 引导入口
; 使用 GRUB 加载，设置栈后跳转到 C++ 内核
;
; 布局说明: multiboot 头固定 24 字节 => _start 位于 0x00100018,
;           MBR 引导器(bootdisk.asm)由此地址进入内核。
; 同时兼容两种启动方式:
;   * GRUB(multiboot): ebx = multiboot info, eax = 0x2BADB002
;   * MBR 自制引导器: 无参数, eax/ebx 为垃圾, 传 (0,0) 给内核

MBALIGN     equ 1<<0
MEMINFO     equ 1<<1
FLAGS       equ MBALIGN | MEMINFO
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    align 16
    dd 0            ; 填充, 使头部共 24 字节 (0x100000 ~ 0x100017)
    dd 0

section .bss
align 16
stack_bottom:
    resb 65536          ; 64KB 栈空间
stack_top:

section .text
global _start
extern kernel_main     ; C++ 内核入口

_start:
    mov esp, stack_top  ; 设置栈指针
    cmp eax, 0x2BADB002 ; 是否从 GRUB (multiboot) 启动?
    jne .nomultiboot
    push ebx            ; 参数2: multiboot info 结构指针 (后压在高地址)
    push eax            ; 参数1: multiboot magic     (先压在低地址/栈顶)
    jmp .go
.nomultiboot:
    push 0              ; 参数2: addr = 0
    push 0              ; 参数1: magic = 0
.go:
    call kernel_main    ; 进入 C++ 内核

    cli
.hang:
    hlt
    jmp .hang
