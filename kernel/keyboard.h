/* keyboard.h — PS/2 键盘驱动 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

class Keyboard {
public:
    static void init();
    static const char* getLine();
    static bool       hasChar();
    static char       getChar();
    static void       resetBuffer();

    // 公开给 IRQ handler 使用
    static char  buffer[256];
    static int   bufHead;
    static int   bufTail;

private:
    static char  scancodeToASCII(uint8_t scancode);
};

#endif
