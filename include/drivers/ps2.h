/*
    ps2.h
    Copyright Menotdan 2018-2019

    Keypress Handler

    MIT License
*/

#pragma once

#include <stdint.h>
#include <string.h>
#include <function.h>
#include <drivers/serial.h>
#include "../../cpu/isr.h"
#include "../../cpu/idt.h"
#include "../../cpu/ports.h"

extern uint8_t err;
void init_ps2();
char get_scancode();