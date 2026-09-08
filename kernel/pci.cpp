/* pci.cpp — 简易 PCI 配置空间访问 (0xCF8/0xCFC) */
#include "pci.h"
#include "port.h"

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

namespace PCI {

    uint32_t readConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
        uint32_t addr = 0x80000000U |
                        ((uint32_t)bus << 16) |
                        ((uint32_t)dev << 11) |
                        ((uint32_t)func << 8) |
                        (reg & 0xFC);
        Port::outd(PCI_CONFIG_ADDR, addr);
        return Port::ind(PCI_CONFIG_DATA);
    }

    void writeConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) {
        uint32_t addr = 0x80000000U |
                        ((uint32_t)bus << 16) |
                        ((uint32_t)dev << 11) |
                        ((uint32_t)func << 8) |
                        (reg & 0xFC);
        Port::outd(PCI_CONFIG_ADDR, addr);
        Port::outd(PCI_CONFIG_DATA, val);
    }

    uint16_t findDevice(uint16_t vendor, uint16_t device) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = readConfig(0, dev, 0, 0);
            if (id == 0xFFFFFFFF || id == 0) continue;
            if ((id & 0xFFFF) == vendor && ((id >> 16) & 0xFFFF) == device) {
                return (uint16_t)((dev << 8) | 0);   // func 0
            }
        }
        return 0xFFFF;
    }
}

/* RTL8139 driver implementation is in rtl8139.cpp */
