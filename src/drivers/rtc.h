#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H
#include <stdint.h>

typedef struct {
    int seconds;
    int minutes;
    int hours;
    int weekday;
    int day_of_month;
    int month;
    int year;
    int century;
} rtc_time_t;

rtc_time_t read_rtc();
void write_rtc(rtc_time_t to_write);
uint64_t get_time_since_epoch();

#endif