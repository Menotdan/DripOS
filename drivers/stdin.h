#pragma once
void key_handler(uint8_t scancode, bool keyup);
void stdin_init();
int keytimeout[256];
static char *key_buffer;