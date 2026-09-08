/* cpu.cpp — CPUID 实现 */
#include "cpu.h"

CPU::Info CPU::probe() {
    Info info = {0};

    // Vendor string
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    info.maxFunc = eax;
    *(uint32_t*)(info.vendor + 0) = ebx;
    *(uint32_t*)(info.vendor + 4) = edx;
    *(uint32_t*)(info.vendor + 8) = ecx;
    info.vendor[12] = '\0';

    // Brand string (if supported)
    if (info.maxFunc >= 0x80000002) {
        for (int i = 0; i < 3; i++) {
            asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002 + i));
            *(uint32_t*)(info.brand + i * 16 + 0)  = eax;
            *(uint32_t*)(info.brand + i * 16 + 4)  = ebx;
            *(uint32_t*)(info.brand + i * 16 + 8)  = ecx;
            *(uint32_t*)(info.brand + i * 16 + 12) = edx;
        }
        info.brand[48] = '\0';
    }

    // Features
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    info.features = edx;

    return info;
}
