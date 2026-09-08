/* serial.h — COM1 串口调试输出 */
#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

class Serial {
public:
    static void init();
    static void putchar(char c);
    static void write(const char* s);
    static void writeln(const char* s);
};

#endif
