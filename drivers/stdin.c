#include "../kernel/kernel.h"
#include "keyboard.h"
#include "../libc/string.h"

#define LSHIFT 0x2A
#define RSHIFT 0x36

//extern bool keydown[256];
static char key_buffer[2560];

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


void key_handler() {
    for (int i = 0; i < 58; i++) {
        if (strcmp(sc_name[i], "Enter") == 0 && keydown[i] == true) {
            kprint("\n");
            user_input(key_buffer); /* kernel-controlled function */
	        uinlen = 0;
            key_buffer[0] = '\0';
        } else if (strcmp(sc_name[i], "Backspace") == 0 && keydown[i] == true) {
            if (uinlen > 0) {
			    backspace(key_buffer);
			    kprint_backspace();
    		    uinlen -= 1;
            }
        } else if (strcmp(sc_name[i], "ERROR") == 0 && keydown[i] == true) {
        } else if (strcmp(sc_name[i], "Lctrl") == 0 && keydown[i] == true) {
        } else if (strcmp(sc_name[i], "LAlt") == 0 && keydown[i] == true) {
        } else if (strcmp(sc_name[i], "LShift") == 0 && keydown[i] == true) {
        } else if (strcmp(sc_name[i], "RShift") == 0 && keydown[i] == true) {
        } else {
            char time[6];
            int_to_ascii(keytimeout[i], time);
            kprint(time);
            if(keytimeout[i] == 0) {
                keytimeout[i] == 500;
                if (keydown[(int)LSHIFT] == true || keydown[(int)RSHIFT] == true) {
                    if(keydown[i] == true) {
                        char letter = sc_ascii_uppercase[i];
                        /* Remember that kprint only accepts char[] */
                        char str[2] = {letter, '\0'};
                        uinlen += 1;
                        append(key_buffer, letter);
                        if (prompttype == 0) {
                            kprint(str);
                        } else {
                            kprint("*");
                        }
                    }
                } else {
                    if(keydown[i] == true) {
                        char letter = sc_ascii[i];
                        /* Remember that kprint only accepts char[] */
                        char str[2] = {letter, '\0'};
                        uinlen += 1;
                        append(key_buffer, letter);
                        if (prompttype == 0) {
                            kprint(str);
                        } else {
		                    kprint("*");
	                    }
                    }
                }
            }
        }
    }
}