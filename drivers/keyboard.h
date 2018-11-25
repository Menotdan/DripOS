#pragma once
#include <stdbool.h>
#include "../cpu/types.h"

extern bool keydown[256];
extern int keytimeout[256];
void init_keyboard();