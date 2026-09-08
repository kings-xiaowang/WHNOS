/* gdt.h — 全局描述符表 */
#ifndef GDT_H
#define GDT_H

#include <stdint.h>

class GDT {
public:
    static void init();

private:
    struct Entry {
        uint16_t limitLow;
        uint16_t baseLow;
        uint8_t  baseMiddle;
        uint8_t  access;
        uint8_t  granularity;
        uint8_t  baseHigh;
    } __attribute__((packed));

    struct Ptr {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    static Entry entries[5];
    static Ptr   ptr;

    static void setEntry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
    static void flush(); // 汇编实现
};

#endif
