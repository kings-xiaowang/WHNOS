/* idt.h — 中断描述符表 */
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

class IDT {
public:
    static void init();
    static void setGate(uint8_t i, uint32_t handler, uint16_t selector, uint8_t flags);

private:
    struct Entry {
        uint16_t low;
        uint16_t selector;
        uint8_t  always0;
        uint8_t  flags;
        uint16_t high;
    } __attribute__((packed));

    struct Ptr {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    static Entry entries[256];
    static Ptr   ptr;
};

#endif
