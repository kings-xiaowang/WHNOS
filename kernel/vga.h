/* vga.h — VGA 文本模式驱动 (80x25, 16色) */
#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum VGAColor {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN   = 14,
    VGA_WHITE         = 15,
};

class VGA {
public:
    static void init();
    static void clear();
    static void putchar(char c);
    static void write(const char* str);
    static void writeLine(const char* str);
    static void setColor(uint8_t fg, uint8_t bg);
    static int  getCursorX();
    static int  getCursorY();
    static void setCursor(int x, int y);

private:
    static uint16_t* buffer;
    static int cursorX;
    static int cursorY;
    static uint8_t color;
    static void scroll();
    static void updateCursor();
};

#endif
