/* isr.h — 中断服务例程声明 */
#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* 中断帧 (由汇编 pushad 等构建)。
 * 压栈顺序（esp 向低地址增长）：
 *   pushad(edi,esi,ebp,esp,ebx,edx,ecx,eax) -> push gs,fs,es,ds
 * 故栈顶(最低地址)是 gs，第一个字段必须是 gs。
 */
struct Registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no;   // 中断号
    uint32_t err_code; // 错误码
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed));

extern "C" {
    // 中断处理函数 (由汇编 ISR 包装器调用)
    void isr_handler(Registers* regs);

    // ISR 入口 (汇编)
    extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
    extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
    extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
    extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
    extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
    extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
    extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
    extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

    // IRQ 入口 (汇编)
    extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
    extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
    extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
    extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();
}

// 注册 ISR/IRQ 到 IDT
void isrInstall();
void irqInstall();

// IRQ 回调注册
typedef void (*IRQHandler)(Registers*);
void registerIRQHandler(uint8_t irq, IRQHandler handler);

#endif
