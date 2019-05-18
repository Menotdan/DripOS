#pragma once
#include <stdint.h>
void read(uint32_t sector);
void copy_sector(uint32_t sector1, uint32_t sector2);
void writeFromBuffer(uint32_t sector);