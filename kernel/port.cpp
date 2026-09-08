/* port.cpp — x86 I/O 端口操作实现 */
#include "port.h"

uint8_t Port::inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

uint16_t Port::inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

uint32_t Port::ind(uint16_t port) {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void Port::outb(uint16_t port, uint8_t data) {
    asm volatile("outb %0, %1" :: "a"(data), "Nd"(port));
}

void Port::outw(uint16_t port, uint16_t data) {
    asm volatile("outw %0, %1" :: "a"(data), "Nd"(port));
}

void Port::outd(uint16_t port, uint32_t data) {
    asm volatile("outl %0, %1" :: "a"(data), "Nd"(port));
}
