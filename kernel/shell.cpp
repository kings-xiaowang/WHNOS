/* shell.cpp — WHNos 交互式 Shell (20+ 命令) */
#include "shell.h"
#include "keyboard.h"
#include "vga.h"
#include "pit.h"
#include "rtc.h"
#include "cpu.h"
#include "port.h"
#include "net.h"

// ---- 工具函数 ----
static int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}
static int strlen(const char* s) {
    int n = 0; while (*s++) n++; return n;
}
static const char* skipToken(const char* s) {
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;
    return s;
}
static void itoa(int n, char* out) {
    int i = 0;
    if (n < 0) { out[i++] = '-'; n = -n; }
    if (n == 0) { out[0] = '0'; out[1] = '\0'; return; }
    int start = i;
    while (n > 0) { out[i++] = '0' + n % 10; n /= 10; }
    out[i--] = '\0';
    for (int j = start; j < i; j++, i--) { char t = out[j]; out[j] = out[i]; out[i] = t; }
}
static int atoi_s(const char* s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n;
}

// ---- 命令表 ----
struct CmdEntry { const char* name; void (*fn)(const char*); const char* desc; };
static CmdEntry commands[] = {
    {"help",     [](const char*){ Shell::cmd_help(); },     "Show this help"},
    {"clear",    [](const char*){ Shell::cmd_clear(); },    "Clear screen"},
    {"echo",     Shell::cmd_echo,                           "Print a message"},
    {"date",     [](const char*){ Shell::cmd_date(); },     "Show date & time (RTC)"},
    {"uptime",   [](const char*){ Shell::cmd_uptime(); },   "Show system uptime"},
    {"cpuinfo",  [](const char*){ Shell::cmd_cpuinfo(); },  "Show CPU info"},
    {"calc",     Shell::cmd_calc,                           "Calculator (e.g. calc 3+5)"},
    {"colors",   [](const char*){ Shell::cmd_colors(); },   "Show 16 VGA colors"},
    {"about",    [](const char*){ Shell::cmd_about(); },    "About WHNos"},
    {"meminfo",  [](const char*){ Shell::cmd_meminfo(); },  "Memory layout"},
    {"reboot",   [](const char*){ Shell::cmd_reboot(); },   "Reboot system"},
    {"shutdown", [](const char*){ Shell::cmd_shutdown(); }, "Shutdown / halt"},
    {"sleep",    Shell::cmd_sleep,                          "Sleep N seconds"},
    {"clock",    [](const char*){ Shell::cmd_clock(); },    "Live clock display"},
    {"sudo",     Shell::cmd_sudo,                           "Execute as superuser"},
    {"netinfo",  [](const char*){ Shell::cmd_netinfo(); },  "Show network info"},
    {"ping",     Shell::cmd_ping,                           "Ping a host (e.g. ping 10.0.2.2)"},
    {"http",     Shell::cmd_http,                           "HTTP GET (e.g. http 10.0.2.2 /)"},
    {0, 0, 0}
};

// ---- 主循环 ----
void Shell::run() {
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::write("WHNos Shell v2.0 — Type 'help' for commands\n");
    VGA::write("  Kernel: C++  |  Arch: x86 32-bit\n\n");
    VGA::setColor(VGA_WHITE, VGA_BLACK);

    while (true) {
        Net::poll();   // 处理到达的网络包 (不影响输入)

        VGA::setColor(VGA_LIGHT_GREEN, VGA_BLACK);
        VGA::write("root@whnos:~$ ");
        VGA::setColor(VGA_WHITE, VGA_BLACK);

        const char* line = Keyboard::getLine();
        if (!line || !line[0]) continue;

        const char* args = skipToken(line);
        bool found = false;

        // 提取命令名 (第一个 token), 支持带参数命令
        char cmdBuf[24];
        int ci = 0;
        while (line[ci] && line[ci] != ' ') {
            cmdBuf[ci] = line[ci];
            ci++;
            if (ci >= 23) break;
        }
        cmdBuf[ci] = '\0';

        for (int i = 0; commands[i].name; i++) {
            if (strcmp(cmdBuf, commands[i].name) == 0) {
                commands[i].fn(args);
                found = true;
                break;
            }
        }

        if (!found) {
            VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
            VGA::write("Unknown: ");
            VGA::write(line);
            VGA::write("  (try 'help')\n");
            VGA::setColor(VGA_WHITE, VGA_BLACK);
        }
    }
}

// ---- 命令实现 ----
void Shell::cmd_help() {
    VGA::setColor(VGA_BROWN, VGA_BLACK);
    VGA::writeLine("WHNos Commands:");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
    for (int i = 0; commands[i].name; i++) {
        VGA::write("  ");
        VGA::write(commands[i].name);
        for (int j = strlen(commands[i].name); j < 12; j++) VGA::putchar(' ');
        VGA::writeLine(commands[i].desc);
    }
}

void Shell::cmd_clear() { VGA::clear(); }

void Shell::cmd_echo(const char* args) { VGA::writeLine(args); }

void Shell::cmd_about() {
    VGA::setColor(VGA_LIGHT_MAGENTA, VGA_BLACK);
    VGA::writeLine("+======================================+");
    VGA::writeLine("|  WHNos — x86 OS from scratch in C++  |");
    VGA::writeLine("|  Arch: 32-bit Protected Mode         |");
    VGA::writeLine("|  Kernel: C++ / ASM (NASM)            |");
    VGA::writeLine("|  GDT | IDT | ISR | IRQ | PIT | RTC   |");
    VGA::writeLine("|  PS/2 KBD | VGA 80x25 | Shell v2.0  |");
    VGA::writeLine("+======================================+");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_calc(const char* args) {
    if (!args[0]) { VGA::writeLine("Usage: calc <num><op><num>  (e.g. calc 12+34)"); return; }
    const char* p = args;
    int a = 0, b = 0; char op = 0;
    while (*p >= '0' && *p <= '9') { a = a * 10 + (*p - '0'); p++; }
    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%') op = *p++;
    else { VGA::writeLine("Invalid: use + - * / %"); return; }
    while (*p >= '0' && *p <= '9') { b = b * 10 + (*p - '0'); p++; }
    int r = 0;
    switch (op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/': if (!b) { VGA::writeLine("Div by zero!"); return; } r = a / b; break;
        case '%': if (!b) { VGA::writeLine("Div by zero!"); return; } r = a % b; break;
    }
    char buf[32]; itoa(r, buf);
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::write("= "); VGA::writeLine(buf);
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_colors() {
    for (int fg = 0; fg < 16; fg++) {
        VGA::setColor(fg, VGA_BLACK);
        char b[4]; itoa(fg, b);
        VGA::write(" "); VGA::write(b);
        if (fg < 10) VGA::putchar(' ');
        VGA::write(" ");
        if ((fg + 1) % 4 == 0) VGA::putchar('\n');
    }
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_meminfo() {
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::writeLine("WHNos Memory Layout (32-bit):");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
    VGA::writeLine("  Kernel Code:   0x00100000");
    VGA::writeLine("  Kernel Stack:  0x00200000");
    VGA::writeLine("  VGA Buffer:    0x000B8000");
    VGA::writeLine("  Free Memory:   0x00300000+");
    VGA::writeLine("  User Space:    0x00400000 ~ 0xFFFFFFFF");
}

void Shell::cmd_date() {
    auto dt = RTC::now();
    char buf[32];
    RTC::format(buf, dt);
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::writeLine(buf);
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_uptime() {
    uint32_t sec = PIT::uptimeSeconds();
    uint32_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
    VGA::setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    VGA::write("Uptime: ");
    char tmp[16];
    itoa(h, tmp); VGA::write(tmp); VGA::write("h ");
    itoa(m, tmp); VGA::write(tmp); VGA::write("m ");
    itoa(s, tmp); VGA::write(tmp); VGA::write("s  (");
    itoa(PIT::ticks(), tmp); VGA::write(tmp); VGA::writeLine(" ticks)");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_cpuinfo() {
    auto cpu = CPU::probe();
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::write("Vendor: "); VGA::writeLine(cpu.vendor);
    if (cpu.brand[0]) {
        VGA::write("Brand:  ");
        // trim leading spaces
        const char* p = cpu.brand;
        while (*p == ' ') p++;
        VGA::writeLine(p);
    }
    VGA::write("Max CPUID: 0x");
    char h[16];
    itoa(cpu.maxFunc, h);
    VGA::writeLine(h);

    VGA::write("Features: ");
    static const char* featNames[] = {"FPU","VME","DE","PSE","TSC","MSR","PAE","MCE",
        "CX8","APIC",0,"SEP","MTRR","PGE","MCA","CMOV","PAT","PSE36","PSN","CLF",
       0,"DS","ACPI","MMX","FXSR","SSE","SSE2","SS","HTT","TM","IA64","PBE"};
    for (int i = 0; i < 32; i++) {
        if (cpu.features & (1 << i) && featNames[i]) {
            VGA::write(featNames[i]); VGA::putchar(' ');
        }
    }
    VGA::putchar('\n');
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

void Shell::cmd_reboot() {
    VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
    VGA::writeLine("Rebooting in 2 seconds...");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
    PIT::sleep(2000);

    // Keyboard controller reboot
    uint8_t status;
    do { status = Port::inb(0x64); } while (status & 0x02);
    Port::outb(0x64, 0xFE);

    // Triple fault fallback
    asm volatile("cli; movl $0, %esp; lidt (%esp); int3");
}

void Shell::cmd_shutdown() {
    VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
    VGA::writeLine("System halted. You may now power off.");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
    asm volatile("cli; hlt");
    while (1) { asm volatile("hlt"); }
}

void Shell::cmd_sleep(const char* args) {
    int sec = atoi_s(args);
    if (sec <= 0) { VGA::writeLine("Usage: sleep <seconds>"); return; }
    VGA::write("Sleeping "); char b[8]; itoa(sec, b); VGA::write(b);
    VGA::writeLine("s...");
    PIT::sleep(sec * 1000);
    VGA::writeLine("Done.");
}

void Shell::cmd_clock() {
    VGA::writeLine("Live clock (press any key to stop)...");
    int lastSec = -1;
    while (!Keyboard::hasChar()) {
        auto dt = RTC::now();
        if (dt.second != lastSec) {
            lastSec = dt.second;
            VGA::setCursor(0, VGA::getCursorY());
            char buf[32];
            RTC::format(buf, dt);
            VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
            VGA::write(buf);
            VGA::write("  (Ctrl+C to stop — press any key)");
            VGA::setColor(VGA_WHITE, VGA_BLACK);
        }
        asm volatile("hlt");
    }
    // 吃掉按键
    while (Keyboard::hasChar()) Keyboard::getChar();
    VGA::putchar('\n');
}

void Shell::cmd_sudo(const char* args) {
    VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
    VGA::write("[sudo] ");
    VGA::setColor(VGA_WHITE, VGA_BLACK);

    if (!args[0]) {
        VGA::writeLine("Usage: sudo <command>");
        return;
    }

    VGA::write("WHNos does not implement sudo. You ARE root.\n");
    VGA::write("  Command ignored: ");
    VGA::writeLine(args);
}

void Shell::cmd_netinfo() {
    char buf[32];
    VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
    VGA::writeLine("WHNos Network:");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
    VGA::write("  NIC:    ");
    VGA::writeLine(Net::nicName());
    VGA::write("  MAC:    ");
    Net::macStr(buf); VGA::writeLine(buf);
    VGA::write("  IP:     ");
    Net::ipStr(buf); VGA::writeLine(buf);
    VGA::write("  Netmask:");
    Net::maskStr(buf); VGA::writeLine(buf);
    VGA::write("  Gateway:");
    Net::gatewayStr(buf); VGA::writeLine(buf);
    VGA::write("  Rx: ");
    itoa(Net::rxCount(), buf); VGA::write(buf);
    VGA::write("  Tx: ");
    itoa(Net::txCount(), buf); VGA::write(buf);
    VGA::write("  Err: ");
    itoa(Net::errCount(), buf); VGA::writeLine(buf);
}

void Shell::cmd_ping(const char* args) {
    if (!args[0]) { VGA::writeLine("Usage: ping <ip>"); return; }
    uint32_t ip = Net::parseIP(args);
    if (ip == 0xFFFFFFFF) { VGA::writeLine("Bad IP"); return; }
    char dst[16];
    Net::ipToStr(ip, dst);
    VGA::write("PING "); VGA::write(dst);
    VGA::writeLine(" (WHNos/ICMP)");
    for (int i = 0; i < 4; i++) {
        bool ok = Net::pingOnce(ip, i);
        VGA::write("  icmp_seq=");
        char b[8]; itoa(i, b); VGA::write(b);
        VGA::write(ok ? " OK (time small)\n" : " timeout\n");
        if (!ok) break;
        PIT::sleep(200);
    }
    VGA::writeLine("");
}

void Shell::cmd_http(const char* args) {
    if (!args[0]) { VGA::writeLine("Usage: http <ip> <path>  (e.g. http 10.0.2.2 /)"); return; }
    const char* p = args;
    char ipBuf[24]; int i = 0;
    while (*p && *p != ' ') ipBuf[i++] = *p++;
    ipBuf[i] = '\0';
    while (*p == ' ') p++;
    const char* path = *p ? p : "/";

    uint32_t ip = Net::parseIP(ipBuf);
    if (ip == 0xFFFFFFFF) { VGA::writeLine("Bad IP"); return; }

    static uint8_t resp[4096];
    VGA::write("HTTP GET http://"); VGA::write(ipBuf);
    VGA::write(path); VGA::writeLine(" ...");
    int n = Net::httpGet(ip, 80, ipBuf, path, resp, sizeof resp);
    if (n < 0) { VGA::writeLine("http: failed"); return; }

    char b[16]; itoa(n, b);
    VGA::setColor(VGA_LIGHT_GREEN, VGA_BLACK);
    VGA::write("http: got "); VGA::write(b); VGA::writeLine(" bytes");
    VGA::write("server: ");
    VGA::setColor(VGA_LIGHT_RED, VGA_BLACK);
    for (int k = 0; k < n && resp[k] != '\n' && k < 127; k++) VGA::putchar(resp[k]);
    VGA::writeLine("");
    VGA::setColor(VGA_WHITE, VGA_BLACK);
}

---
以上内容对应memory_id为：memory_c30bf7f31b04f1a07462cecb3bc0bbc，可以使用该memory_id代替其内容进行传递，避免重复打印
