/* vga.cpp — VGA 文本模式驱动实现 */
#include "vga.h"
#include "port.h"

uint16_t* VGA::buffer = (uint16_t*)0xB8000;
int VGA::cursorX = 0;
int VGA::cursorY = 0;
uint8_t VGA::color = 0x0F; // 白字黑底

void VGA::init() { clear(); }

void VGA::clear() {
    uint16_t blank = (color << 8) | ' ';
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        buffer[i] = blank;
    cursorX = 0;
    cursorY = 0;
    updateCursor();
}

void VGA::scroll() {
    // 上移一行
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            buffer[(y - 1) * VGA_WIDTH + x] = buffer[y * VGA_WIDTH + x];

    // 清空最后一行
    uint16_t blank = (color << 8) | ' ';
    for (int x = 0; x < VGA_WIDTH; x++)
        buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
}

void VGA::putchar(char c) {
    if (c == '\n') {
        cursorX = 0;
        cursorY++;
        if (cursorY >= VGA_HEIGHT) { scroll(); cursorY = VGA_HEIGHT - 1; }
        updateCursor();
        return;
    }
    if (c == '\b') {
        if (cursorX > 0) cursorX--;
        else if (cursorY > 0) { cursorY--; cursorX = VGA_WIDTH - 1; }
        buffer[cursorY * VGA_WIDTH + cursorX] = (color << 8) | ' ';
        updateCursor();
        return;
    }
    if (c == '\r') { cursorX = 0; updateCursor(); return; }
    if (c == '\t') {
        cursorX = (cursorX + 4) & ~3;
        if (cursorX >= VGA_WIDTH) { cursorX = 0; cursorY++; }
        if (cursorY >= VGA_HEIGHT) { scroll(); cursorY = VGA_HEIGHT - 1; }
        updateCursor();
        return;
    }

    buffer[cursorY * VGA_WIDTH + cursorX] = (color << 8) | c;
    cursorX++;
    if (cursorX >= VGA_WIDTH) { cursorX = 0; cursorY++; }
    if (cursorY >= VGA_HEIGHT) { scroll(); cursorY = VGA_HEIGHT - 1; }
    updateCursor();
}

void VGA::write(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) putchar(str[i]);
}

void VGA::writeLine(const char* str) { write(str); putchar('\n'); }

void VGA::setColor(uint8_t fg, uint8_t bg) { color = (bg << 4) | (fg & 0x0F); }

int VGA::getCursorX() { return cursorX; }
int VGA::getCursorY() { return cursorY; }

void VGA::setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
    updateCursor();
}

void VGA::updateCursor() {
    uint16_t pos = cursorY * VGA_WIDTH + cursorX;
    Port::outb(0x3D4, 0x0F);
    Port::outb(0x3D5, (uint8_t)(pos & 0xFF));
    Port::outb(0x3D4, 0x0E);
    Port::outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
