#ifndef COLOR_MATH_H
#define COLOR_MATH_H

#define V_MIN 0
#define H_MAX 255
#define S_MIN 0

typedef struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColor;

typedef struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
} HsvColor;

RgbColor HsvToRgb(HsvColor hsv);
HsvColor RgbToHsv(RgbColor rgb);

#endif