/* serial.cpp — COM1 串口调试输出实现 */
#include "serial.h"
#include "port.h"

// COM1 端口基地址
#define COM1 0x3F8

void Serial::init() {
    Port::outb(COM1 + 1, 0x00);  // 关闭中断
    Port::outb(COM1 + 3, 0x80);  // 启用 DLAB
    Port::outb(COM1 + 0, 0x03);  // 115200 波特率低字节
    Port::outb(COM1 + 1, 0x00);  // 115200 波特率高字节
    Port::outb(COM1 + 3, 0x03);  // 8N1
    Port::outb(COM1 + 2, 0xC7);  // 启用 FIFO
    Port::outb(COM1 + 4, 0x0B);  // DTR+RTS
}

static int isTransmitEmpty() {
    return Port::inb(COM1 + 5) & 0x20;
}

void Serial::putchar(char c) {
    while (!isTransmitEmpty()) {}
    Port::outb(COM1, (uint8_t)c);
}

void Serial::write(const char* s) {
    while (*s) {
        if (*s == '\n') Serial::putchar('\r');
        Serial::putchar(*s++);
    }
}

void Serial::writeln(const char* s) {
    write(s);
    putchar('\r');
    putchar('\n');
}
