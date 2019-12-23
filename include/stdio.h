/*
    stdio.h
    Copyright Menotdan 2018-2019

    Standard Input/Output definitions

    MIT License
*/

#pragma once

#include <drivers/screen.h>
#include <kernel/kernel.h>

void printf(char *text);
void stdin_call(char *text);
char* scanf(char *message);