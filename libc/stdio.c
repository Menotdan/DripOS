/*
    stdio.c
    Copyright Menotdan 2018-2019

    Standard Input/Output definitions

    MIT License
*/

#include <stdio.h>

int in = 0;
char *keyinput;
int stdinpass = 0;

// TODO: rework this so its not just a wrapper
void printf(char* text) {
    kprint(text);
}

void stdin_call(char* text) {
    in = 1;
    keyinput = text;
}

char* scanf(char* message) {
    kprint(message);
    stdinpass = 1;
    in = 0;
    keyinput=0;
    return keyinput;
}