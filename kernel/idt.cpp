/* idt.cpp — IDT 实现 */
#include "idt.h"

IDT::Entry IDT::entries[256];
IDT::Ptr   IDT::ptr;

void IDT::setGate(uint8_t i, uint32_t handler, uint16_t selector, uint8_t flags) {
    entries[i].low      = handler & 0xFFFF;
    entries[i].selector = selector;
    entries[i].always0  = 0;
    entries[i].flags    = flags;
    entries[i].high     = (handler >> 16) & 0xFFFF;
}

extern "C" void idt_flush(uint32_t);

void IDT::init() {
    ptr.limit = sizeof(entries) - 1;
    ptr.base  = (uint32_t)&entries;

    // 通过 idt_flush 加载
    idt_flush((uint32_t)&ptr);
}
