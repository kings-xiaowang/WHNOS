/* pit.cpp — PIT 实现 */
#include "pit.h"
#include "port.h"
#include "isr.h"

volatile uint32_t PIT::tickCount = 0;
uint32_t PIT::frequency = 0;

static void pitHandler(Registers*) {
    PIT::tickCount++;
}

void PIT::init(uint32_t freq) {
    frequency = freq;
    tickCount = 0;

    uint32_t divisor = 1193180 / freq;
    Port::outb(0x43, 0x36);
    Port::outb(0x40, divisor & 0xFF);
    Port::outb(0x40, (divisor >> 8) & 0xFF);

    registerIRQHandler(0, pitHandler);
}

uint32_t PIT::ticks()            { return tickCount; }
uint32_t PIT::uptimeSeconds()    { return tickCount / frequency; }

void PIT::sleep(uint32_t ms) {
    uint32_t target = tickCount + ms * frequency / 1000;
    while (tickCount < target) { asm volatile("hlt"); }
}
