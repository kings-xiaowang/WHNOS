/* keyboard.cpp — PS/2 键盘驱动实现 */
#include "keyboard.h"
#include "isr.h"
#include "port.h"
#include "vga.h"

char Keyboard::buffer[256];
int  Keyboard::bufHead = 0;
int  Keyboard::bufTail = 0;

static bool shift = false;
static bool caps  = false;

// 美式键盘扫描码 → ASCII
static const char keymap[128] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,  ' ',0,
    // F1-F10
    0,0,0,0,0,0,0,0,0,0,
    // num lock, scroll lock, home, up, page up, -, left, center, right, +, end, down, page down, ins, del
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char keymapShift[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    0,0,' ',0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void irqKeyboard(Registers*) {
    uint8_t scancode = Port::inb(0x60);
    char c = 0;

    if (scancode == 0x2A || scancode == 0x36) shift = true;
    else if (scancode == 0xAA || scancode == 0xB6) shift = false;
    else if (scancode == 0x3A) caps = !caps;
    else if (scancode < 128) {
        c = shift ? keymapShift[scancode] : keymap[scancode];
        if (caps && c >= 'a' && c <= 'z') c -= 32;
    }

    if (c) {
        int next = (Keyboard::bufTail + 1) % 256;
        if (next != Keyboard::bufHead) {
            Keyboard::buffer[Keyboard::bufTail] = c;
            Keyboard::bufTail = next;
        }
    }
}

void Keyboard::init() {
    registerIRQHandler(1, irqKeyboard);
}

bool Keyboard::hasChar() {
    return bufHead != bufTail;
}

char Keyboard::getChar() {
    if (bufHead == bufTail) return 0;
    char c = buffer[bufHead];
    bufHead = (bufHead + 1) % 256;
    return c;
}

void Keyboard::resetBuffer() {
    bufHead = 0;
    bufTail = 0;
}

const char* Keyboard::getLine() {
    static char line[256];
    int i = 0;
    while (i < 255) {
        char c = getChar();
        if (!c) {
            // 等待输入 (HALT 降低 CPU 占用)
            asm volatile("hlt");
            continue;
        }
        if (c == '\n') { line[i] = '\0'; VGA::putchar('\n'); break; }
        if (c == '\b') {
            if (i > 0) { i--; VGA::putchar('\b'); }
            continue;
        }
        VGA::putchar(c);
        line[i++] = c;
    }
    line[i] = '\0';
    return line;
}
