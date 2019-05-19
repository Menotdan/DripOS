#include "../kernel/kernel.h"
#include "keyboard.h"
#include "../libc/string.h"
#include <stdint.h>
#include <stdbool.h>
#include "../drivers/screen.h"

#define LSHIFT 0x2A
#define RSHIFT 0x36
#define KEY_DEL 2

static char *key_buffer;

const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = { ' ', ' ', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', ' ', ' ', 'q', 'w', 'e', 'r', 't', 'y', 
        'u', 'i', 'o', 'p', '[', ']', ' ', ' ', 'a', 's', 'd', 'f', 'g', 
        'h', 'j', 'k', 'l', ';', '\'', '`', ' ', '\\', 'z', 'x', 'c', 'v', 
        'b', 'n', 'm', ',', '.', '/', ' ', ' ', ' ', ' '};

const char sc_ascii_uppercase[] = { ' ', ' ', '!', '@', '#', '$', '%', '^',     
    '&', '*', '(', ')', '_', '+', ' ', ' ', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '{', '}', ' ', ' ', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ':', '|', '~', ' ', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', '<', '>', '?', ' ', ' ', ' ', ' '};

int shift = 0;
int first = 1;

void key_handler(uint8_t scancode, bool keyup) {
    if (strcmp(sc_name[scancode], "Backspace") == 0 && keyup != true) {
        if (uinlen > 0) {
            backspace(key_buffer);
            kprint_backspace();
            uinlen -= 1;
        }
    } else if (strcmp(sc_name[scancode], "ERROR") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "Lctrl") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "LAlt") == 0 && keyup != true) {
    } else if (strcmp(sc_name[scancode], "LShift") == 0 && keyup != true) {
        shift = 1;
    } else if (strcmp(sc_name[scancode], "RShift") == 0 && keyup != true) {
        shift = 1;
    }  else if (strcmp(sc_name[scancode], "LShift") == 0 && keyup == true) {
        shift = 0;
    } else if (strcmp(sc_name[scancode], "RShift") == 0 && keyup == true) {
        shift = 0;
    } else {
        if (shift == 0) {
            if (!keyup) {
                append(key_buffer, sc_ascii[scancode]);
                kprint(sc_ascii[scancode]);
            }
        } else if (shift == 1) {
            if (!keyup) {
                append(key_buffer, sc_ascii[scancode]);
                kprint(sc_ascii[scancode]);
            }
        }
    }
}
void stdin_init() {
    backspace(key_buffer);
    uinlen = 0;
    key_buffer = 0;
}