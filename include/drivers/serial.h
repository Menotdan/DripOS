/*
    serial.h
    Copyright Menotdan 2018-2019

    Write to serial ports

    MIT License
*/

#pragma once

#include <stdio.h>
#include <string.h>
#include "../../cpu/timer.h"

void init_serial();
void write_serial(char a);
void sprint(char* message);
void sprintd(char* message);