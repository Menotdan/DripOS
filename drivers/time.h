#ifndef TIME_H
#define TIME_H
#define CURRENT_YEAR 2018

extern int century_register;
extern unsigned char second;
extern unsigned char minute;
extern unsigned char hour;
extern unsigned char day;
extern unsigned char month;
extern unsigned int year;
enum cmos {
	cmos_address = 0x70,
	cmos_data    = 0x71,
};
int get_update_in_progress_flag();
unsigned char get_RTC_register(int reg);
void format_time(char *string_buf, int len);
void read_rtc();
#endif