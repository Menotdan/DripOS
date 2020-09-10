#ifndef KLIBC_LOGGER_H
#define KLIBC_LOGGER_H
#include "drivers/serial.h"
#include "klibc/string.h"

void log(char *message, ...);
void log_alloc(char *message, ...);

#endif