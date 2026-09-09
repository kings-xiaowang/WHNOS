/* pkg.cpp — WHNos 简易包管理器 (类 apt 最小实现, 内存模式) */
#include "pkg.h"
#include "net.h"
#include "vga.h"
#include "serial.h"
#include "pit.h"

namespace Pkg {

#define PKG_MAX_INDEX  4096   // 索引文件 (Packages) 最大字节
#define PKG_MAX_PKGS   16     // 包表容量
#define PKG_MAX_NAME    32    // 包名最大长度
#define PKG_MAX_CONTENT 4096  // 单个包内容缓冲 (安装到内存)

    struct PkgEntry {
        char   name[PKG_MAX_NAME];
        char   ver[16];
        uint32_t size;
        uint32_t checksum;
        char   path[80];
        bool   installed;
    };

    static PkgEntry gPkgs[PKG_MAX_PKGS];
    static int  gCount = 0;
    static char gErr[96] = "none";

    // 已安装包内容缓冲 (简单起见: 最近安装的那个)
    static uint8_t gContent[PKG_MAX_CONTENT];
    static int  gContentLen = 0;

    // ---- 工具 ----
    static int mystrlen(const char* s) {
        int n = 0; while (s[n]) n++; return n;
    }
    static int mystrcmp(const char* a, const char* b) {
        while (*a && *a == *b) { a++; b++; }
        return (unsigned char)*a - (unsigned char)*b;
    }
    static long myatoi(const char* s) {
        long v = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
        return v;
    }
    // 十六进制字符串 -> u32
    static uint32_t hexToU32(const char* s) {
        uint32_t v = 0;
        while (*s) {
            int d = -1;
            if (*s >= '0' && *s <= '9') d = *s - '0';
            else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
            else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
            if (d < 0) break;
            v = (v << 4) | (uint32_t)d;
            s++;
        }
        return v;
    }
    static void copyStr(char* dst, const char* src, int cap) {
        int i = 0;
        while (*src && i < cap - 1) { *dst++ = *src++; i++; }
        *dst = '\0';
    }
    // 复制一个 token (到空格/制表/换行/串尾为止)
    static void copyWord(char* dst, const char* src, int cap) {
        int i = 0;
        while (*src && *src != ' ' && *src != '\t' && *src != '\r' &&
               *src != '\n' && i < cap - 1) { *dst++ = *src++; i++; }
        *dst = '\0';
    }
    // FNV-1a 32 位校验
    static uint32_t fnv1a(const uint8_t* d, int len) {
        uint32_t h = 2166136261u;
        for (int i = 0; i < len; i++) {
            h ^= d[i];
            h *= 16777619u;
        }
        return h;
    }

    // 从 HTTP 响应中提取 body (跳过 "HTTP/1.1 200 OK\r\n...\r\n\r\n")
    // 返回 body 起始指针, *outLen 为 body 长度; 找不到分隔返回原始数据。
    static const uint8_t* httpBody(const uint8_t* data, int len, int* outLen) {
        for (int i = 0; i + 3 < len; i++) {
            if (data[i] == '\r' && data[i+1] == '\n' &&
                data[i+2] == '\r' && data[i+3] == '\n') {
                *outLen = len - (i + 4);
                return data + i + 4;
            }
        }
        *outLen = len;
        return data;
    }

    // 解析一行 "name ver size checksum path"
    static bool parseLine(const char* line, PkgEntry* e) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') return false;          // 空行/注释
        // name
        const char* fields[5]; int fi = 0;
        fields[fi++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        // 依次读取剩余 4 个字段 (fields[0] 已是 name)
        int f = 0;
        for (f = 1; f < 5; f++) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            fields[f] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        if (f < 5) return false;

        copyWord(e->name, fields[0], PKG_MAX_NAME);
        copyWord(e->ver, fields[1], 16);
        e->size = (uint32_t)myatoi(fields[2]);
        e->checksum = hexToU32(fields[3]);
        copyWord(e->path, fields[4], 80);
        e->installed = false;
        // size 至少 1, path 以 '/' 开头
        if (e->size == 0 || e->path[0] != '/') return false;
        return true;
    }

    int count() { return gCount; }
    const char* lastError() { return gErr; }

    int update(const char* host, uint16_t port) {
        static uint8_t indexBuf[PKG_MAX_INDEX];
        gCount = 0;

        int n = Net::httpGet(Net::parseIP(host), port, host, "/Packages",
                             indexBuf, sizeof indexBuf);
        if (n < 0) {
            copyStr(gErr, "http get /Packages failed", sizeof gErr);
            Serial::writeln("pkg: update http failed");
            return PKG_UPDATE_FAIL;
        }

        int bodyLen = 0;
        const uint8_t* body = httpBody(indexBuf, n, &bodyLen);

        // 按行解析
        char line[120]; int li = 0;
        int okCount = 0;
        for (int i = 0; i <= bodyLen; i++) {
            char c = (i < bodyLen) ? (char)body[i] : '\n';
            if (c == '\n') {
                line[li] = '\0';
                if (parseLine(line, &gPkgs[okCount])) {
                    if (okCount < PKG_MAX_PKGS - 1) okCount++;
                }
                li = 0;
            } else {
                if (li < (int)sizeof line - 1) line[li++] = c;
            }
        }
        gCount = okCount;
        if (gCount == 0) {
            copyStr(gErr, "no packages parsed", sizeof gErr);
            return PKG_UPDATE_FAIL;
        }
        copyStr(gErr, "none", sizeof gErr);
        Serial::writeln("pkg: index parsed");
        return PKG_UPDATE_OK;
    }

    void list(bool onlyInstalled) {
        if (gCount == 0) {
            VGA::writeLine("pkg: empty (run: pkg update <ip> [port])");
            return;
        }
        VGA::setColor(VGA_LIGHT_CYAN, VGA_BLACK);
        VGA::writeLine("name                 ver    size   checksum  inst");
        VGA::setColor(VGA_WHITE, VGA_BLACK);
        for (int i = 0; i < gCount; i++) {
            PkgEntry* e = &gPkgs[i];
            if (onlyInstalled && !e->installed) continue;
            VGA::write("  ");
            VGA::write(e->name);
            for (int j = mystrlen(e->name); j < 21; j++) VGA::putchar(' ');
            VGA::write(e->ver);
            for (int j = mystrlen(e->ver); j < 8; j++) VGA::putchar(' ');
            {
                char b[12]; int bi = 0;
                uint32_t v = e->size;
                do { b[bi++] = (char)('0' + v % 10); v /= 10; } while (v);
                for (int k = bi - 1; k >= 0; k--) VGA::putchar(b[k]);
                for (int k = bi; k < 7; k++) VGA::putchar(' ');
            }
            {
                static const char* HX = "0123456789ABCDEF";
                for (int k = 7; k >= 0; k--) VGA::putchar(HX[(e->checksum >> (k * 4)) & 0xF]);
            }
            VGA::putchar(' ');
            VGA::write(e->installed ? "YES" : "no");
            VGA::putchar('\n');
        }
    }

    int install(const char* name, const char* host, uint16_t port) {
        if (!name || !name[0]) { copyStr(gErr, "usage: pkg install <name>", sizeof gErr); return -1; }
        if (gCount == 0) { copyStr(gErr, "no index (run pkg update first)", sizeof gErr); return -1; }

        PkgEntry* e = 0;
        for (int i = 0; i < gCount; i++) {
            if (mystrcmp(gPkgs[i].name, name) == 0) { e = &gPkgs[i]; break; }
        }
        if (!e) { copyStr(gErr, "package not found", sizeof gErr); return -1; }
        if (e->installed) { copyStr(gErr, "already installed", sizeof gErr); return -1; }
        if (e->size > PKG_MAX_CONTENT) {
            copyStr(gErr, "package too big", sizeof gErr);
            return -1;
        }

        VGA::write("pkg: fetching ");
        VGA::write(e->path);
        VGA::write(" ...\n");

        int n = Net::httpGet(Net::parseIP(host), port, host, e->path,
                             gContent, sizeof gContent);
        if (n < 0) { copyStr(gErr, "download failed", sizeof gErr); return -1; }

        int bodyLen = 0;
        const uint8_t* body = httpBody(gContent, n, &bodyLen);
        if ((uint32_t)bodyLen != e->size) {
            copyStr(gErr, "size mismatch", sizeof gErr);
            return -1;
        }
        uint32_t chk = fnv1a(body, bodyLen);
        if (chk != e->checksum) {
            copyStr(gErr, "checksum mismatch", sizeof gErr);
            return -1;
        }
        e->installed = true;
        gContentLen = bodyLen;
        copyStr(gErr, "none", sizeof gErr);

        VGA::write("pkg: installed ");
        VGA::write(e->name);
        VGA::write(" v");
        VGA::write(e->ver);
        VGA::write(" (");
        char b[12]; int bi = 0;
        uint32_t v = (uint32_t)bodyLen;
        do { b[bi++] = (char)('0' + v % 10); v /= 10; } while (v);
        for (int k = bi - 1; k >= 0; k--) VGA::putchar(b[k]);
        VGA::writeLine(" bytes)");
        return 0;
    }

    int cat(const char* name, int amount) {
        if (!name) return -1;
        // 定位内容: 最近安装的即 cat 的对象 (单缓冲简化)
        bool found = false;
        for (int i = 0; i < gCount; i++) {
            if (mystrcmp(gPkgs[i].name, name) == 0 && gPkgs[i].installed) { found = true; break; }
        }
        if (!found) { copyStr(gErr, "not installed", sizeof gErr); return -1; }
        if (amount <= 0 || amount > gContentLen) amount = gContentLen;
        for (int i = 0; i < amount; i++) {
            char c = (char)gContent[i];
            if (c == '\n' || c == '\r' || (c >= 32 && c < 127)) VGA::putchar(c);
            else VGA::putchar('.');
        }
        VGA::putchar('\n');
        return amount;
    }
}
