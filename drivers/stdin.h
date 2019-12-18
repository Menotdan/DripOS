#pragma once
//void key_handler(uint8_t scancode, bool keyup);
//void stdin_init();
//int keytimeout[256];
extern char *sc_name[];
extern char sc_ascii[];
extern char sc_ascii_uppercase[];
char scancode_to_ascii(char scan, uint8_t upper);
char getch(uint8_t upper);
char *getline(uint8_t upper);
char *getline_print(uint8_t upper);
char getcode();
#define LSHIFT 0x2A
#define RSHIFT 0x36
#define LARROW 0x4B
#define RARROW 0x4D
#define UPARROW 0x48
#define DOWNARROW 0x50
#define KEY_DEL 2
#define SC_MAX 57