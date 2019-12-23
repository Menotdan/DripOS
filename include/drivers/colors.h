/*
    stdio.h
    Copyright Menotdan 2018-2019

    Standard Input/Output definitions

    MIT License
*/

#pragma once

#include <stdint.h>

#define WHITE_ON_BLACK 0x0f
#define RED_ON_WHITE 0xf4
#define BLUE_ON_BLACK 0x09
#define WHITE_ON_WHITE 0xff
#define CYAN_ON_CYAN 0x33
#define BLACK_ON_BLACK 0x00

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);

inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t) c | ((uint16_t) color << 8u);
}