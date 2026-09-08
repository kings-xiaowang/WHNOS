/* rtc.h — Real-Time Clock (CMOS) */
#ifndef RTC_H
#define RTC_H

#include <stdint.h>

class RTC {
public:
    struct DateTime {
        uint8_t second, minute, hour, weekday, day, month;
        uint16_t year;
    };

    static DateTime now();
    static void     format(char* buf, const DateTime& dt);  // "2026-07-13 16:30:00"
};

#endif
