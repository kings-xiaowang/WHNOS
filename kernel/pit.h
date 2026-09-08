/* pit.h — Programmable Interval Timer (PIT) */
#ifndef PIT_H
#define PIT_H

#include <stdint.h>

class PIT {
public:
    static void init(uint32_t freq);   // freq: Hz, 典型值 1000
    static uint32_t ticks();           // 自启动以来的 tick 数
    static void sleep(uint32_t ms);    // 阻塞延时 (毫秒)
    static uint32_t uptimeSeconds();   // 运行秒数

    // 公开给 IRQ handler
    static volatile uint32_t tickCount;

private:
    static uint32_t frequency;
};

#endif
