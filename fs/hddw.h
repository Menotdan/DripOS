#pragma once
#include <stdint.h>
void read(uint32_t sector);
void copy_sector(uint32_t sector1, uint32_t sector2);
void writeFromBuffer(uint32_t sector);
void init_hddw();
void clear_sector(uint32_t sector);
uint16_t readOut[256];
uint16_t writeIn[256];
uint8_t *readBuffer;