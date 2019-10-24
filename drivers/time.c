#include "time.h"
 #include "../cpu/ports.h"
 int century_register;
 unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int  year;
 enum cmos {
	cmos_address = 0x70,
	cmos_data    = 0x71,
};
 int get_update_in_progress_flag() {
	port_byte_out(cmos_address, 0x0A);
	return (port_byte_in(cmos_data) & 0x80);
}
 unsigned char get_RTC_register(int reg) {
	port_byte_out(cmos_address, reg);
	return port_byte_in(cmos_data);
}
 void read_rtc() {
	unsigned char century;
	unsigned char last_second;
	unsigned char last_minute;
	unsigned char last_hour;
	unsigned char last_day;
	unsigned char last_month;
	unsigned char last_year;
	unsigned char last_century;
	unsigned char registerB;
 	//while(get_update_in_progress_flag());
 	second = get_RTC_register(0x00);
	minute = get_RTC_register(0x02);
	hour = get_RTC_register(0x04);
	day = get_RTC_register(0x07);
	month = get_RTC_register(0x08);
	year = get_RTC_register(0x09);
	century = 0;
 	if(century_register != 0) {
		century = get_RTC_register(century_register);
	}
 	do {
		last_second  = second;
		last_minute  = minute;
		last_hour    = hour;
		last_day     = day;
		last_month   = month;
		last_year    = year;
		last_century = century;
 		while(get_update_in_progress_flag());
 		second = get_RTC_register(0x00);
		minute = get_RTC_register(0x02);
		hour = get_RTC_register(0x04);
		day = get_RTC_register(0x07);
		month = get_RTC_register(0x08);
		year = get_RTC_register(0x09);
 		if(century_register != 0) {
			century = get_RTC_register(century_register);
		}
	} while((last_second != second) || (last_minute != minute) ||
		(last_hour != hour) || (last_day != day) ||
		(last_month != month) || (last_year != year) || 
		(last_century != century_register));
 	registerB = get_RTC_register(0x0B);
 	if(!(registerB & 0x04)) {
		second = (second & 0x0F) + ((second / 16) * 10);
		minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
    day = (day & 0x0F) + ((day / 16) * 10);
    month = (month & 0x0F) + ((month / 16) * 10);
    year = (year & 0x0F) + ((year / 16) * 10);
    
    if(century_register != 0) {
      century = (century & 0x0F) + ((century / 16) * 10);
    }
	}
 	if(!(registerB & 0x02) && (hour & 0x80)) {
		hour = ((hour & 0x7F) + 12) % 24;
	}
 	if(century_register != 0) {
		year += century * 100;
	} else {
		year += (CURRENT_YEAR / 100) * 100;
		if(year < CURRENT_YEAR) year += 100;
	}
} 