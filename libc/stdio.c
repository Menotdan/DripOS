#include "../drivers/screen.h"
#include "../libc/stdio.h"
#include "../kernel/kernel.h"
int in = 0;
char *keyinput;
void printf(char *text) {
    kprint(text);
}

void stdin_call(char *text) {
    in = 1;
    keyinput = text;
}

char * scanf(char *message) {
    kprint(message);
    stdinpass = 1;
    in = 0;
    keyinput=0;
    return keyinput;
}