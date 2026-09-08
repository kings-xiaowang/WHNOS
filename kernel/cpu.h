/* cpu.h — CPU 信息 (CPUID) */
#ifndef CPU_H
#define CPU_H

#include <stdint.h>

class CPU {
public:
    struct Info {
        char vendor[13];
        char brand[49];
        uint32_t maxFunc;
        uint32_t features;
    };

    static Info probe();
};

#endif
