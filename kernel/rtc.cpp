/* rtc.cpp — RTC 实现 (CMOS 读取) */
#include "rtc.h"
#include "port.h"

static uint8_t cmosRead(uint8_t reg) {
    Port::outb(0x70, reg);
    return Port::inb(0x71);
}

static uint8_t bcd2bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

RTC::DateTime RTC::now() {
    DateTime dt;

    // 等待更新结束
    while (cmosRead(0x0A) & 0x80);

    dt.second  = bcd2bin(cmosRead(0x00));
    dt.minute  = bcd2bin(cmosRead(0x02));
    dt.hour    = bcd2bin(cmosRead(0x04));
    dt.weekday = bcd2bin(cmosRead(0x06));
    dt.day     = bcd2bin(cmosRead(0x07));
    dt.month   = bcd2bin(cmosRead(0x08));
    dt.year    = bcd2bin(cmosRead(0x09)) + 2000;

    return dt;
}

void RTC::format(char* buf, const DateTime& dt) {
    // "2026-07-13 16:30:00"
    auto w2 = [&](int n, int& i) {
        buf[i++] = '0' + n / 10;
        buf[i++] = '0' + n % 10;
    };
    auto w4 = [&](int n, int& i) {
        buf[i++] = '0' + n / 1000;
        buf[i++] = '0' + (n / 100) % 10;
        buf[i++] = '0' + (n / 10) % 10;
        buf[i++] = '0' + n % 10;
    };

    int i = 0;
    w4(dt.year, i);   buf[i++] = '-';
    w2(dt.month, i);  buf[i++] = '-';
    w2(dt.day, i);    buf[i++] = ' ';
    w2(dt.hour, i);   buf[i++] = ':';
    w2(dt.minute, i); buf[i++] = ':';
    w2(dt.second, i);
    buf[i] = '\0';
}
