#pragma once
#include <stdbool.h>
#include "../cpuPt2/types.h"
#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36
#define LARROW 76
#define RARROW 78
#define KEYUPOFFSET 0x80
#define SC_MAX 57

//extern int keytimeout[256];
void init_keyboard();