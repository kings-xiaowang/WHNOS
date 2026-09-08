/* gdt.cpp — GDT 实现 */
#include "gdt.h"

GDT::Entry GDT::entries[5];
GDT::Ptr   GDT::ptr;

void GDT::setEntry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    entries[i].baseLow     = base & 0xFFFF;
    entries[i].baseMiddle  = (base >> 16) & 0xFF;
    entries[i].baseHigh    = (base >> 24) & 0xFF;
    entries[i].limitLow    = limit & 0xFFFF;
    entries[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    entries[i].access      = access;
}

extern "C" void gdt_flush(uint32_t);

void GDT::init() {
    ptr.limit = sizeof(entries) - 1;
    ptr.base  = (uint32_t)&entries;

    // 段选择子: 0=null, 1=code, 2=data, 3=user_code, 4=user_data
    setEntry(0, 0, 0, 0, 0);                      // null
    setEntry(1, 0, 0xFFFFF, 0x9A, 0xCF);          // 内核代码段
    setEntry(2, 0, 0xFFFFF, 0x92, 0xCF);          // 内核数据段
    setEntry(3, 0, 0xFFFFF, 0xFA, 0xCF);          // 用户代码段
    setEntry(4, 0, 0xFFFFF, 0xF2, 0xCF);          // 用户数据段

    gdt_flush((uint32_t)&ptr);
}
