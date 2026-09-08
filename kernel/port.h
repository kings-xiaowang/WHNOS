/* port.h — x86 I/O 端口操作 */
#ifndef PORT_H
#define PORT_H

#include <stdint.h>

class Port {
public:
    static uint8_t  inb(uint16_t port);
    static uint16_t inw(uint16_t port);
    static uint32_t ind(uint16_t port);

    static void outb(uint16_t port, uint8_t  data);
    static void outw(uint16_t port, uint16_t data);
    static void outd(uint16_t port, uint32_t data);
};

#endif
