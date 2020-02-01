#include "color_math.h"
#include <stdint.h>
#include "../drivers/vesa.h"

RgbColor HsvToRgb(HsvColor hsv)  {
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;

    if (hsv.s == 0){
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

HsvColor RgbToHsv(RgbColor rgb) {
    HsvColor hsv;
    unsigned char rgbMin, rgbMax;

    rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

    hsv.v = rgbMax;
    if (hsv.v == 0) {
        hsv.h = 0;
        hsv.s = 0;
        return hsv;
    }

    hsv.s = 255 * (long) (rgbMax - rgbMin) / hsv.v;
    if (hsv.s == 0) {
        hsv.h = 0;
        return hsv;
    }

    if (rgbMax == rgb.r)
        hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
    else if (rgbMax == rgb.g)
        hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
    else
        hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

    return hsv;
}

// void color_prog() {
    // uint8_t v = 255;
    // uint8_t h = 0;
    // uint8_t s = 255;
    // while (1) {
    //     HsvColor color_dat;
    //     color_dat.h = h;
    //     color_dat.s = s;
    //     color_dat.v = v;
    //     for (uint64_t pixel = 0; pixel < mbd->framebuffer_width * mbd->framebuffer_height; pixel++) {
    //         RgbColor rgb_dat = HsvToRgb(color_dat);
    //         //sprintf("\nHSV: %u %u %u\nRGB: %u %u %u", (uint32_t) color_dat.h, (uint32_t) color_dat.s, (uint32_t) color_dat.v, (uint32_t) rgb_dat.r, (uint32_t) rgb_dat.g, (uint32_t) rgb_dat.b);
    //         //sprintf("\n\nwriting to pos: %lx", mbd->framebuffer_addr + (pixel * 4));
    //         *((uint32_t *) (mbd->framebuffer_addr + (pixel * 4))) = (uint32_t) ((uint32_t) rgb_dat.r << mbd->framebuffer_red_field_position) | ((uint32_t) rgb_dat.g << mbd->framebuffer_green_field_position) | ((uint32_t) rgb_dat.b << mbd->framebuffer_blue_field_position);
    //         if (color_dat.h >= H_MAX) {
    //             color_dat.h = 0;
    //         } else {
    //             color_dat.h++;
    //         }
    //         if (color_dat.s == S_MIN) {
    //             color_dat.s = 255;
    //         } else {
    //             color_dat.s--;
    //         }
    //         if (color_dat.v == V_MIN) {
    //             color_dat.v = 255;
    //         } else {
    //             color_dat.v--;
    //         }
    //     }
    //     if (h >= H_MAX) {
    //         h = 0;
    //     } else {
    //         h++;
    //     }
    // }
// }