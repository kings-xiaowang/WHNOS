/* isr.cpp — ISR 处理 + IRQ 处理实现 */
#include "isr.h"
#include "idt.h"
#include "vga.h"
#include "port.h"

// ISR 异常名称
static const char* exceptions[] = {
    "Division by Zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Seg", "Invalid TSS", "Segment Not Present",
    "Stack-Seg Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FP Error",
    "Virtualization", "Control Protection", "", "",
    "", "", "", "",
    "Hypervisor Injection", "VMM Communication", "Security", ""
};

void isr_handler(Registers* regs) {
    VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
    VGA::write("[EXCEPTION] ");
    VGA::setColor(VGA_WHITE, VGA_BLACK);

    if (regs->int_no < 32 && exceptions[regs->int_no][0]) {
        VGA::write(exceptions[regs->int_no]);
    } else {
        VGA::write("ISR ");
        char buf[10];
        // 简单数字输出
        int n = regs->int_no;
        int i = 0;
        if (n == 0) { buf[i++] = '0'; } else {
            while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
            // 反转
            for (int j = 0; j < i / 2; j++) {
                char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t;
            }
        }
        buf[i] = '\0';
        VGA::write(buf);
    }
    VGA::putchar('\n');

    // 严重异常停止
    if (regs->int_no < 32) {
        VGA::setColor(VGA_BROWN, VGA_BLACK);
        VGA::write("System halted.\n");
        VGA::setColor(VGA_WHITE, VGA_BLACK);
        asm volatile("cli; hlt");
    }
}

// 注册 ISR
void isrInstall() {
    IDT::setGate(0,  (uint32_t)isr0,  0x08, 0x8E);
    IDT::setGate(1,  (uint32_t)isr1,  0x08, 0x8E);
    IDT::setGate(2,  (uint32_t)isr2,  0x08, 0x8E);
    IDT::setGate(3,  (uint32_t)isr3,  0x08, 0x8E);
    IDT::setGate(4,  (uint32_t)isr4,  0x08, 0x8E);
    IDT::setGate(5,  (uint32_t)isr5,  0x08, 0x8E);
    IDT::setGate(6,  (uint32_t)isr6,  0x08, 0x8E);
    IDT::setGate(7,  (uint32_t)isr7,  0x08, 0x8E);
    IDT::setGate(8,  (uint32_t)isr8,  0x08, 0x8E);
    IDT::setGate(9,  (uint32_t)isr9,  0x08, 0x8E);
    IDT::setGate(10, (uint32_t)isr10, 0x08, 0x8E);
    IDT::setGate(11, (uint32_t)isr11, 0x08, 0x8E);
    IDT::setGate(12, (uint32_t)isr12, 0x08, 0x8E);
    IDT::setGate(13, (uint32_t)isr13, 0x08, 0x8E);
    IDT::setGate(14, (uint32_t)isr14, 0x08, 0x8E);
    IDT::setGate(15, (uint32_t)isr15, 0x08, 0x8E);
    IDT::setGate(16, (uint32_t)isr16, 0x08, 0x8E);
    IDT::setGate(17, (uint32_t)isr17, 0x08, 0x8E);
    IDT::setGate(18, (uint32_t)isr18, 0x08, 0x8E);
    IDT::setGate(19, (uint32_t)isr19, 0x08, 0x8E);
    IDT::setGate(20, (uint32_t)isr20, 0x08, 0x8E);
    IDT::setGate(21, (uint32_t)isr21, 0x08, 0x8E);
    IDT::setGate(22, (uint32_t)isr22, 0x08, 0x8E);
    IDT::setGate(23, (uint32_t)isr23, 0x08, 0x8E);
    IDT::setGate(24, (uint32_t)isr24, 0x08, 0x8E);
    IDT::setGate(25, (uint32_t)isr25, 0x08, 0x8E);
    IDT::setGate(26, (uint32_t)isr26, 0x08, 0x8E);
    IDT::setGate(27, (uint32_t)isr27, 0x08, 0x8E);
    IDT::setGate(28, (uint32_t)isr28, 0x08, 0x8E);
    IDT::setGate(29, (uint32_t)isr29, 0x08, 0x8E);
    IDT::setGate(30, (uint32_t)isr30, 0x08, 0x8E);
    IDT::setGate(31, (uint32_t)isr31, 0x08, 0x8E);
}

// ---------- IRQ ----------
static IRQHandler irqHandlers[16] = {0};

void registerIRQHandler(uint8_t irq, IRQHandler handler) {
    irqHandlers[irq] = handler;
}

extern "C" void irq_handler(Registers* regs) {
    // EOI
    if (regs->int_no >= 40)
        Port::outb(0xA0, 0x20);
    Port::outb(0x20, 0x20);

    if (irqHandlers[regs->int_no - 32])
        irqHandlers[regs->int_no - 32](regs);
}

void irqInstall() {
    // 重映射 PIC: IRQ 0-7 → 0x20-0x27, IRQ 8-15 → 0x28-0x2F
    Port::outb(0x20, 0x11);
    Port::outb(0xA0, 0x11);
    Port::outb(0x21, 0x20);
    Port::outb(0xA1, 0x28);
    Port::outb(0x21, 0x04);
    Port::outb(0xA1, 0x02);
    Port::outb(0x21, 0x01);
    Port::outb(0xA1, 0x01);
    Port::outb(0x21, 0x00);
    Port::outb(0xA1, 0x00);

    IDT::setGate(32, (uint32_t)irq0,  0x08, 0x8E);
    IDT::setGate(33, (uint32_t)irq1,  0x08, 0x8E);
    IDT::setGate(34, (uint32_t)irq2,  0x08, 0x8E);
    IDT::setGate(35, (uint32_t)irq3,  0x08, 0x8E);
    IDT::setGate(36, (uint32_t)irq4,  0x08, 0x8E);
    IDT::setGate(37, (uint32_t)irq5,  0x08, 0x8E);
    IDT::setGate(38, (uint32_t)irq6,  0x08, 0x8E);
    IDT::setGate(39, (uint32_t)irq7,  0x08, 0x8E);
    IDT::setGate(40, (uint32_t)irq8,  0x08, 0x8E);
    IDT::setGate(41, (uint32_t)irq9,  0x08, 0x8E);
    IDT::setGate(42, (uint32_t)irq10, 0x08, 0x8E);
    IDT::setGate(43, (uint32_t)irq11, 0x08, 0x8E);
    IDT::setGate(44, (uint32_t)irq12, 0x08, 0x8E);
    IDT::setGate(45, (uint32_t)irq13, 0x08, 0x8E);
    IDT::setGate(46, (uint32_t)irq14, 0x08, 0x8E);
    IDT::setGate(47, (uint32_t)irq15, 0x08, 0x8E);
}
