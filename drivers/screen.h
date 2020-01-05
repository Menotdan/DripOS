#ifndef SCREEN_H
#define SCREEN_H

#include "../cpu/types.h"
#include "../cpu/isr.h"
#include "vesa.h"
typedef unsigned char *va_list;
/* width of stack == width of int */
#define	STACKITEM	int
 
/* round up width of objects pushed on stack. The expression before the
& ensures that we get 0 for objects of size 0. */
#define	VA_SIZE(TYPE)					\
	((sizeof(TYPE) + sizeof(STACKITEM) - 1)	\
		& ~(sizeof(STACKITEM) - 1))
/* &(LASTARG) points to the LEFTMOST argument of the function call
(before the ...) */
#define	va_start(AP, LASTARG)	\
	(AP=((va_list)&(LASTARG) + VA_SIZE(LASTARG)))
/* nothing for va_end */
#define va_end(AP)
#define va_arg(AP, TYPE)	\
	(AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))
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