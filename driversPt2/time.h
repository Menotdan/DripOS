#ifndef TIME_H
#define TIME_H
 #define CURRENT_YEAR 2018
 int century_register;
 unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int  year;
 enum cmos;
 int get_update_in_progress_flag();
unsigned char get_RTC_register(int reg);
 void format_time(char *string_buf, int len);
void read_rtc();
#endif