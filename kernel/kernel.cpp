/* kernel.cpp — C++ 内核入口 */
#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "keyboard.h"
#include "pit.h"
#include "net.h"
#include "shell.h"

extern "C" void kernel_main(unsigned int magic, unsigned int addr) {
    // 0. 初始化串口调试
    Serial::init();

    // 1. 初始化 GDT (必须先于 VGA)
    GDT::init();

    // 2. 初始化 VGA 文本显示
    VGA::init();
    VGA::setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    VGA::writeLine("Booting WHNos...");

    // 3. 安装 IDT 并注册 ISR
    IDT::init();
    isrInstall();
    VGA::writeLine("IDT & ISR installed.");

    // 4. 重映射 PIC、安装 IRQ
    irqInstall();
    VGA::writeLine("IRQ remapped.");

    // 5. 初始化键盘
    Keyboard::init();
    VGA::writeLine("Keyboard ready.");

    // 6. 初始化 PIT 计时器 (1000Hz)
    PIT::init(1000);
    VGA::writeLine("PIT timer (1000Hz).");

    // 7. 开中断
    asm volatile("sti");
    VGA::writeLine("Interrupts enabled.");

    // 8. 初始化网络子系统 (PCI 枚举 + 网卡驱动 + 协议基础)
    Net::init();

    // 9. 进入 Shell
    VGA::writeLine("");
    Shell::run();

    // 不会到达这里
    while (1) { asm volatile("hlt"); }
}
