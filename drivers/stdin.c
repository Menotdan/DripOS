#include "../kernel/kernel.h"
#include "keyboard.h"
#include "../libc/string.h"
#include <stdint.h>
#include <stdbool.h>
#include <serial.h>
#include <debug.h>
#include "../drivers/screen.h"
#include "../libc/mem.h"

#define LSHIFT 0x2A
#define RSHIFT 0x36
#define LARROW 0x4B
#define RARROW 0x4D
#define UPARROW 0x48
#define DOWNARROW 0x50
#define KEY_DEL 2
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
    if (scancode == LARROW && keyup != true){
        if (position > 0) {
            position -= 1;
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)-1, get_offset_row(cOffset)));
        }
    } else if (scancode == RARROW && keyup != true){
        if (position < uinlen) {
            position += 1;
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)+1, get_offset_row(cOffset)));
        }
    } else if (scancode == UPARROW && keyup != true){
        uint32_t loop = 0;
        uint32_t loop2 = 0;
        while (loop2 < strlen(key_buffer))
        {
            key_buffer_down[loop] = key_buffer[loop];
            loop2++;
        }
        
        while (loop < strlen(key_buffer_up))
        {
            key_buffer[loop] = key_buffer_up[loop];
            loop++;
        }

        uint32_t offsetTemp = get_cursor_offset();
        int cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
        for (uint32_t g = 0; g < uinlen; g++)
        {
            kprint_backspace();
        }
        //kprint_backspace();
        cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset), get_offset_row(cOffset)));
        if (key_buffer != 0) {
            kprint(key_buffer);
        }
        //sprintd(key_buffer);
        //set_cursor_offset(get_offset(get_offset_col(offsetTemp)-1, get_offset_row(offsetTemp)));
        uinlen = strlen(key_buffer_up);
        position = uinlen;
    } 
    // else if (scancode == DOWNARROW && keyup != true){
    //     uint32_t loop = 0;
    //     uint32_t loop2 = 0;

    //     // while (loop2 < strlen(key_buffer))
    //     // {
    //     //     key_buffer_up[loop] = key_buffer[loop];
    //     //     loop2++;
    //     // }
        
    //     key_buffer[loop] = "\0";

    //     uint32_t offsetTemp = get_cursor_offset();
    //     int cOffset = get_cursor_offset();
    //     set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
    //     for (uint32_t g = 0; g < uinlen; g++)
    //     {
    //         kprint_backspace();
    //     }
    //     //kprint_backspace();
    //     cOffset = get_cursor_offset();
    //     set_cursor_offset(get_offset(get_offset_col(cOffset), get_offset_row(cOffset)));
    //     kprint(key_buffer);
    //     //sprintd(key_buffer);
    //     //set_cursor_offset(get_offset(get_offset_col(offsetTemp)-1, get_offset_row(offsetTemp)));
    //     uinlen = strlen(key_buffer_down);
    //     position = uinlen;
    // }
    else if (strcmp(sc_name[scancode], "Backspace") == 0 && keyup != true) {
        if (uinlen > 0) {
            backspacep(key_buffer, position);
            uint32_t offsetTemp = get_cursor_offset();
            int cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
            for (uint32_t g = 0; g < uinlen; g++)
            {
                kprint_backspace();
            }
            //kprint_backspace();
            cOffset = get_cursor_offset();
            set_cursor_offset(get_offset(get_offset_col(cOffset), get_offset_row(cOffset)));
            if (key_buffer != 0) {
                kprint(key_buffer);
            }
            //sprintd(key_buffer);
            set_cursor_offset(get_offset(get_offset_col(offsetTemp)-1, get_offset_row(offsetTemp)));
            uinlen -= 1;
            position -= 1;
        }
    } else if (strcmp(sc_name[scancode], "Spacebar") == 0 && keyup != true) {
        appendp(key_buffer, sc_ascii[scancode], position);
        uint32_t offsetTemp = get_cursor_offset();
        int cOffset = get_cursor_offset();
        set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
        for (uint32_t g = 0; g < uinlen; g++)
        {
            kprint_backspace();
        }
        cOffset = get_cursor_offset();
        if (key_buffer != 0) {
            kprint(key_buffer);
        }
        set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
        uinlen++;
        position++;
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
    } else if (strcmp(sc_name[scancode], "Enter") == 0 && keyup != true) {
        //fixer(key_buffer);
        //user_input(key_buffer);
        for (int q = 0; q < uinlen; q++) {
            key_buffer_up[q] = key_buffer[q];
            key_buffer_up[q+1] = '\0';
        }
        uint32_t l = strlen(key_buffer);
        key_buffer[l] = 3;
        key_buffer[l+1] = "\0";
    } else {
        if (shift == 0) {
            if (!keyup) {
                if (scancode < SC_MAX){
                    appendp(key_buffer, sc_ascii[scancode], position);
                    char str[2] = {sc_ascii[scancode], '\0'};
                    //kprint(str);
                    uint32_t offsetTemp = get_cursor_offset();
                    int cOffset = get_cursor_offset();
                    breakA();
                    set_cursor_offset(get_offset(get_offset_col(cOffset)+(uinlen-position), get_offset_row(cOffset)));
                    breakA();
                    cOffset = get_cursor_offset();
                    for (uint32_t g = 0; g < uinlen; g++)
                    {
                        //set_cursor_offset(get_offset(get_offset_col(cOffset)+(uinlen-position) - g, get_offset_row(cOffset)));
                        kprint_backspace();
                        //kprint_at(0x08, -1, -1);
                        breakA();
                    }
                    //kprint_backspace();
                    //kprint_backspace();
                    
                    breakA();
                    if (key_buffer != 0) {
                        kprint(key_buffer);
                    }
                    set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
                    //sprintd(key_buffer);
                    uinlen++;
                    position++;
                }
            }
        } else if (shift == 1) {
            if (!keyup) {
                if (scancode < SC_MAX){
                    //append(key_buffer, sc_ascii_uppercase[scancode]);
                    appendp(key_buffer, sc_ascii_uppercase[scancode], position);
                    char str[2] = {sc_ascii_uppercase[scancode], '\0'};

                    uint32_t offsetTemp = get_cursor_offset();
                    int cOffset = get_cursor_offset();
                    set_cursor_offset(get_offset(get_offset_col(cOffset)+uinlen-position, get_offset_row(cOffset)));
                    for (uint32_t g = 0; g < uinlen; g++)
                    {
                        kprint_backspace();
                    }
                    //kprint_backspace();
                    cOffset = get_cursor_offset();
                    if (key_buffer != 0) {
                        kprint(key_buffer);
                    }
                    //sprintd(key_buffer);

                    //kprint(str);
                    set_cursor_offset(get_offset(get_offset_col(offsetTemp)+1, get_offset_row(offsetTemp)));
                    uinlen++;
                    position++;
                }
            }
        }
    }
}
void stdin_init() {
    //backspace(key_buffer);
    uinlen = 0;
    key_buffer[0] = 0;
    key_buffer_down[0] = 0;
    key_buffer_up[0] = 0;
}