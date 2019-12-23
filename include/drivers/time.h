/*
    time.h
    Copyright Menotdan 2018-2019

    CMOS Time driver

    MIT License
*/

#pragma once

#include <stdint.h>
#include "../../cpu/ports.h"

#define CURRENT_YEAR 2020

extern int32_t century_register;
extern uint8_t second;
extern uint8_t minute;
extern uint8_t hour;
extern uint8_t day;
extern uint8_t month;
extern uint32_t year;

enum cmos {
	cmos_address = 0x70,
	cmos_data    = 0x71,
};

int get_update_in_progress_flag();
uint8_t get_RTC_register(int reg);
void format_time(char *string_buf, int len);
void read_rtc();