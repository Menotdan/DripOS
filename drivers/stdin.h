#pragma once
//void key_handler(uint8_t scancode, bool keyup);
//void stdin_init();
//int keytimeout[256];
char scancode_to_ascii(char scan, uint8_t upper);
char getch(uint8_t upper);
char *getline(uint8_t upper);
char *getline_print(uint8_t upper);