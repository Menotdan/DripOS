/*
    math.c
    Copyright Menotdan 2018-2019

    Math functions

    MIT License
*/

#include <math.h>

#include <stdint.h> //what the hell. it doesn't work without this

uint32_t abs(int num) {
    if ((num) < 0) return (uint32_t)-num;
    return (uint32_t)num;
}