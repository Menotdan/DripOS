#include "rtc.h"
#include <io/ports.h>
#include <drivers/tty/tty.h>

rtc_time_t read_rtc() {
    rtc_time_t ret;

    port_outb(0x70, 0xA);
    io_wait();
    while (port_inb(0x71) & (1<<7)) {
        asm("hlt");
    }

    port_outb(0x70, 0x0);
    io_wait();
    ret.seconds = port_inb(0x71);

    port_outb(0x70, 0x2);
    io_wait();
    ret.minutes = port_inb(0x71);

    port_outb(0x70, 0x4);
    io_wait();
    ret.hours = port_inb(0x71);

    port_outb(0x70, 0x6);
    io_wait();
    ret.weekday = port_inb(0x71);

    port_outb(0x70, 0x7);
    io_wait();
    ret.day_of_month = port_inb(0x71);

    port_outb(0x70, 0x8);
    io_wait();
    ret.month = port_inb(0x71);

    port_outb(0x70, 0x9);
    io_wait();
    ret.year = port_inb(0x71);

    ret.century = 20;

    return ret;
}

void write_rtc(rtc_time_t to_write) {
    port_outb(0x70, 0xA);
    io_wait();
    while (port_inb(0x71) & (1<<7)) {
        asm("hlt");
    }

    port_outb(0x70, 0x0);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.seconds);

    port_outb(0x70, 0x2);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.minutes);

    port_outb(0x70, 0x4);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.hours);

    port_outb(0x70, 0x6);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.weekday);

    port_outb(0x70, 0x7);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.day_of_month);

    port_outb(0x70, 0x8);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.month);

    port_outb(0x70, 0x9);
    io_wait();
    port_outb(0x71, (uint8_t) to_write.year);
}

uint64_t rtc_time_to_seconds(rtc_time_t time) {
    uint64_t sec = 0;
    sec += time.seconds;
    sec += time.minutes * 60;
    sec += time.hours * 60 * 60;

    uint64_t day_of_year = time.day_of_month - 1;
    uint64_t months_start = 0;
    switch (time.month) {
        case 1:
            months_start = 0;
            break;
        case 2:
            months_start = 31;
            break;
        case 3:
            months_start = 59;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 4:
            months_start = 90;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 5:
            months_start = 120;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 6:
            months_start = 151;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 7:
            months_start = 181;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 8:
            months_start = 212;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 9:
            months_start = 243;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 10:
            months_start = 273;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 11:
            months_start = 304;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        case 12:
            months_start = 334;
            if (time.year % 4 == 0) {
                months_start += 1;
            }
            break;
        default:
            break;
    }

    day_of_year += months_start;
    sec += day_of_year * 24 * 60 * 60;

    uint64_t year = time.year + (time.century * 100);
    uint64_t days_in_years = (year * 365) + (year / 4);
    sec += days_in_years * 24 * 60 * 60;

    return sec;
}

uint64_t get_time_since_epoch() {
    rtc_time_t current_time = read_rtc();
    rtc_time_t epoch_time = {0, 0, 0, 5, 1, 1, 70, 19};
    uint64_t current_seconds = rtc_time_to_seconds(current_time);
    uint64_t epoch_seconds = rtc_time_to_seconds(epoch_time);
    return current_seconds - epoch_seconds;
}