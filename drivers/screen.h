#ifndef SCREEN_H
#define SCREEN_H

#include "../cpu/types.h"
#include "../cpu/isr.h"
#include "vesa.h"

#define VIDEO_ADDRESS 0xb8000
// #define WHITE_ON_BLACK 0x0f
// #define RED_ON_WHITE 0xf4
// #define BLUE_ON_BLACK 0x09

/* Screen i/o ports */
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

/* Public kernel API */
void clear_screen();
void kprint_at(char *message, int col, int row);
void kprint(char *message);
void kprintf(char *message, ...);
void kprint_uint_no_update(unsigned int num);
void kprint_int_no_update(int num);
void kprint_uint(unsigned int num);
void kprint_int(int num);
void kprint_no_update(char *message);
void kprint_backspace();
void kprint_at_blue(char *message, int col, int row);
void kprint_no_move(char *message, int col, int row);
void logo_draw();
int get_offset_col(int offset);
int get_offset_row(int offset);
int get_offset(int col, int row);
void set_cursor_offset(int offset);
int get_cursor_offset();
void kprint_color(char *message, color_t fg, color_t bg);
void crash_screen(registers_t *crash_state, char *msg, uint8_t printReg);
void setup_screen();
char *screen_chars;


#endif