/* pci.h — 简易 PCI 配置空间访问 */
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_VENDOR_REALTEK 0x10EC
#define PCI_DEV_RTL8139    0x8139

namespace PCI {

    uint32_t readConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
    void     writeConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val);

    // 在 bus 0 上扫描所有设备, 返回匹配 vendor/device 的 bdf (dev<<8|func);
    // 未找到返回 0xFFFF
    uint16_t findDevice(uint16_t vendor, uint16_t device);
}

#endif
