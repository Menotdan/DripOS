#include "../kernel/kernel.h"
#include "keyboard.h"
#include "../libc/string.h"
#include <stdint.h>
#include <stdbool.h>
#include <serial.h>
#include <debug.h>
#include "../drivers/screen.h"
#include "../libc/mem.h"
#include "ps2.h"
#include "../cpu/task.h"

const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = { ' ', ' ', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', ' ', ' ', 'q', 'w', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '[', ']', '\n', ' ', 'a', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', ';', '\'', '`', ' ', '\\', 'z', 'x', 'c', 'v', 
        'b', 'n', 'm', ',', '.', '/', ' ', ' ', ' ', ' '};

const char sc_ascii_uppercase[] = { ' ', ' ', '!', '@', '#', '$', '%', '^',     
    '&', '*', '(', ')', '_', '+', ' ', ' ', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '{', '}', '\n', ' ', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ':', '|', '~', ' ', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', '<', '>', '?', ' ', ' ', ' ', ' '};

//int shift = 0;
//int first = 1;


// void stdin_init() {
//     //backspace(key_buffer);
//     uinlen = 0;
//     key_buffer[0] = 0;
//     key_buffer_down[0] = 0;
//     key_buffer_up[0] = 0;
// }

char scancode_to_ascii(char scan, uint8_t upper) {
    if (scan < SC_MAX) {
        if (upper == 1) {
            return (char)sc_ascii_uppercase[(uint8_t)scan];
        } else {
            return (char)sc_ascii[(uint8_t)scan];
        }
    } else {
        return ' ';
    }
}

char getch(uint8_t upper) {
    char ret;
    ret = get_scancode();
    while ((ret == 0 && err == 1) || (uint8_t)ret > SC_MAX) {
        err = 0;
        runningTask->state = IRQ_WAIT;
        runningTask->waiting = 1;
        yield();
        ret = get_scancode();
    }
    ret = scancode_to_ascii(ret, upper);
    return ret;
}

char getcode() {
    char ret;
    ret = get_scancode();
    while ((ret == 0 && err == 1)) {
        err = 0;
        runningTask->state = IRQ_WAIT;
        runningTask->waiting = 1;
        yield();
        ret = get_scancode();
    }
    return ret;
}

char *getline(uint8_t upper) {
    char *buffer = kmalloc(2000);
    char *start_pointer = buffer;
    char currentChar = '\0';
    *buffer = 0;
    currentChar = getch(upper);
    while (currentChar != '\n') {
        *buffer = currentChar;
        buffer++;
        currentChar = getch(upper);
    }
    *buffer = '\0';
    return start_pointer;
}

char *getline_print(uint8_t upper) {
    char *buffer = kmalloc(2000);
    char *start_pointer = buffer;
    char currentChar[2];
    *buffer = 0;
    currentChar[1] = '\0';
    currentChar[0] = getch(upper);
    while (currentChar[0] != '\n') {
        kprint((char *)&currentChar);
        *buffer = currentChar[0];
        buffer++;
        currentChar[0] = getch(upper);
    }
    *buffer = '\0';
    kprint((char *)&currentChar);
    return start_pointer;
}