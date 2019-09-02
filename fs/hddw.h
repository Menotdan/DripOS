#pragma once
#include "../libc/stdint.h"
void read(uint32_t sector);
void copy_sector(uint32_t sector1, uint32_t sector2);
void writeFromBuffer(uint32_t sector);
void empty_sector();
void clear_sector(uint32_t sector);
uint16_t readOut[256];
uint16_t writeIn[256];