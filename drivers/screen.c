#include "screen.h"
#include "../cpu/ports.h"
#include "../cpu/types.h"
#include <stddef.h>
#include "../libc/mem.h"
#include "colors.h"
#include "../libc/string.h"
#include "../cpu/isr.h"
#include "../cpu/task.h"
#include "../kernel/kernel.h"

uint32_t screen_offset = 0;
color_t white;
color_t black;
color_t red;
color_t green;
color_t blue;
color_t cyan;
char *screen_chars;

/* Declaration of private functions */
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, color_t fg, color_t bg);
int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_col(int offset);

/**********************************************************
 * Public Kernel API functions                            *
 **********************************************************/

/**
 * Print a message on the specified location
 * If col, row, are negative, we will use the current offset
 */

int logo[16][16] = {
        {0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 3, 3, 1, 1, 3, 3, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 3, 1, 2, 2, 1, 3, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 3, 3, 1, 2, 2, 1, 3, 3, 0, 0, 0, 0},
        {0, 0, 0, 0, 3, 1, 2, 2, 2, 2, 1, 3, 0, 0, 0, 0},
        {0, 0, 0, 3, 3, 1, 2, 2, 2, 2, 1, 3, 3, 0, 0, 0},
        {0, 0, 0, 3, 1, 2, 2, 2, 2, 2, 2, 1, 3, 0, 0, 0},
        {0, 0, 3, 3, 1, 2, 2, 2, 2, 2, 2, 1, 3, 3, 0, 0},
        {0, 0, 3, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 0, 0},
        {0, 0, 3, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 0, 0},
        {0, 0, 3, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 0, 0},
        {0, 0, 3, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 0, 0},
        {0, 0, 3, 3, 1, 2, 2, 2, 2, 2, 2, 1, 3, 3, 0, 0},
        {0, 0, 0, 3, 3, 1, 2, 2, 2, 2, 1, 3, 3, 0, 0, 0},
        {0, 0, 0, 0, 3, 3, 1, 1, 1, 1, 3, 3, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0}
};

void setup_screen() {
    white.red = 255;
    white.green = 255;
    white.blue = 255;
    
    black.red = 0;
    black.green = 0;
    black.blue = 0;

    red.red = 255;
    red.green = 0;
    red.blue = 0;

    green.red = 0;
    green.green = 255;
    green.blue = 0;

    blue.red = 0;
    blue.green = 0;
    blue.blue = 255;

    cyan.red = 0;
    cyan.green = 255;
    cyan.blue = 255;

    screen_chars = kmalloc((char_w*char_h));
    memory_set((uint8_t*)screen_chars, 0, (char_w*char_h));
}

void kprint_at(char *message, int col, int row) {
    /* Set cursor if col/row are negative */
    int offset;
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }

    /* Loop through message and print it */
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, white, black);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
    update_display();
}

void kprint_no_move(char *message, int col, int row) {
    /* Set cursor if col/row are negative */
    int offset;
    int prevOffset;
    prevOffset = get_cursor_offset();
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }

    /* Loop through message and print it */
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, white, black);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
    set_cursor_offset(prevOffset);
    update_display();
}

void kprint_at_blue(char *message, int col, int row) {
    /* Set cursor if col/row are negative */
    int offset;
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }

    /* Loop through message and print it */
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, white, black);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void kprint_at_col(char *message, int col, int row, color_t fg, color_t bg) {
    /* Set cursor if col/row are negative */
    int offset;
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }

    /* Loop through message and print it */
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, fg, bg);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void kprint(char *message) {
    kprint_at(message, -1, -1);
}

void kprint_color(char *message, color_t fg, color_t bg) {
    kprint_at_col(message, -1, -1, fg, bg);
}

void crash_screen(registers_t *crash_state, char *msg, uint8_t printReg) {
    asm volatile("cli");
    clear_screen();
    set_cursor_offset(get_offset(35, 0));
    kprint_color("ERROR", red, black);
    set_cursor_offset(get_offset(15, 1));
    kprint("The system has been halted to prevent damage");
    set_cursor_offset(get_offset(15, 3));
    kprint(msg);
    if (printReg == 1) {
        set_cursor_offset(get_offset(30, 11));
        kprint("eip: ");
        kprint_uint(crash_state->eip);
        set_cursor_offset(get_offset(5, 12));
        kprint("eax: ");
        kprint_uint(crash_state->eax);
        kprint("   ebx: ");
        kprint_uint(crash_state->ebx);
        kprint("   ecx: ");
        kprint_uint(crash_state->ecx);
        kprint("   edx: ");
        kprint_uint(crash_state->edx);
        set_cursor_offset(get_offset(5, 13));
        kprint("edi: ");
        kprint_uint(crash_state->edi);
        kprint("   esi: ");
        kprint_uint(crash_state->esi);
        kprint("   esp: ");
        kprint_uint(crash_state->esp);
        kprint("   ebp: ");
        kprint_uint(crash_state->ebp);
        set_cursor_offset(get_offset(14, 14));
        kprint("cs: ");
        kprint_uint(crash_state->cs);
        kprint("   ds: ");
        kprint_uint(crash_state->ds);
        //kprint("   ss: ");
        //kprint_uint(crash_state->ss);
        kprint("\nTask eip: ");
        kprint_uint(runningTask->regs.eip);
        kprint("\nTask eax: ");
        kprint_uint(runningTask->regs.eax);
        kprint("\nTask ebx: ");
        kprint_uint(runningTask->regs.ebx);
        kprint("\nTask ecx: ");
        kprint_uint(runningTask->regs.ecx);
        kprint("\nTask edx: ");
        kprint_uint(runningTask->regs.edx);
        kprint("\nDebug 6: ");
        kprint_uint(crash_state->dr6);
        kprint("\nEFLAGS: ");
        kprint_uint(crash_state->eflags);
        kprint("\nOOF: ");
        kprint_uint(oof);
        kprint(" EAX: ");
        kprint_uint(eax);
        kprint(" ESP: ");
        kprint_uint(esp);
        kprint("\nEIP: ");
        kprint_uint(eip);
        kprint("\nError code: ");
        kprint_uint(crash_state->err_code);
    }
    asm("hlt");
}

void logoDraw() {
    int xOff = 24;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if(logo[y][x] == 1) {
                kprint_at_col(" ", x + xOff, y, black, black);
            } else if(logo[y][x] == 2) {                kprint_at_col(" ", x + xOff, y, cyan, cyan);
            } else if(logo[y][x] == 3) {
                kprint_at_col(" ", x + xOff, y, white, white);
            } else {
                kprint_at_col(" ", x + xOff, y, white, black);
            }
        }
    }
}

void kprint_backspace() {
    int offset = get_cursor_offset()-2;
    int row = get_offset_row(offset);
    int col = get_offset_col(offset);
    print_char(0x08, col, row, white, black);
}


/**********************************************************
 * Private kernel functions                               *
 **********************************************************/


/**
 * Innermost print function for our kernel, directly accesses the video memory 
 *
 * If 'col' and 'row' are negative, we will print at current cursor location
 * If 'attr' is zero it will use 'white on black' as default
 * Returns the offset of the next character
 * Sets the video cursor to the returned offset
 */
int print_char(char c, int col, int row, color_t fg, color_t bg) {
    uint32_t MAX_COLS = char_w;
    uint32_t MAX_ROWS = char_h;

    /* Error control: print a red 'E' if the coords aren't right */
    if ((uint32_t)col >= MAX_COLS || (uint32_t)row >= MAX_ROWS) {
        return get_offset(col, row);
    }

    uint32_t offset;
    if (col >= 0 && row >= 0) offset = (uint32_t)get_offset(col, row);
    else offset = get_cursor_offset();

    if (c == '\n') {
        row = get_offset_row(offset);
        offset = get_offset(0, row+1);
    } else if (c == 0x8) {
        screen_chars[(get_offset(col,row))/2] = ' ';
        render8x8bitmap(font8x8_basic[(uint32_t)c], col, row, bg, fg);
    } else {
        screen_chars[(get_offset(col,row))/2] = c;
        render8x8bitmap(font8x8_basic[(uint32_t)c], col, row, bg, fg);
        offset += 2;
    }

    /* Check if the offset is over screen size and scroll */
    if (offset >= MAX_ROWS * MAX_COLS * 2) {
        uint32_t i;
        for (i = 1; i < MAX_ROWS; i++) {
            for (uint32_t r = 0; r < MAX_COLS; r++) {
                screen_chars[(get_offset(i-1, r))/2] = screen_chars[(get_offset(i, r))/2];
            }
        }
        for (i = 0; i < MAX_ROWS; i++) {
            for (uint32_t r = 0; r < MAX_COLS; r++) {
                if (screen_chars[get_offset(i, r)/2] != '\0') {
                    render8x8bitmap(font8x8_basic[(uint8_t)screen_chars[get_offset(i, r)/2]], col, row, bg, fg);
                }
            }
        }
        offset -= 2 * MAX_COLS;
    }

    set_cursor_offset(offset);
    return offset;
}

int get_cursor_offset() {
    return (int)screen_offset; /* Position * size of character cell */
}

void set_cursor_offset(int offset) {
    /* Similar to get_cursor_offset, but instead of reading we write data */
    screen_offset = (uint32_t)offset;
}

void clear_screen() {
    set_cursor_offset(0);
    fill_screen(0,0,0);
    memory_set((uint8_t*)screen_chars, 0, (char_w*char_h));
}


int get_offset(int col, int row) { 
    uint32_t MAX_COLS = char_w;
    return 2 * (row * MAX_COLS + col);
}
int get_offset_row(int offset) {
    uint32_t MAX_COLS = char_w;
    return offset / (2 * MAX_COLS);
}
int get_offset_col(int offset) {
    uint32_t MAX_COLS = char_w;
    return (offset - (get_offset_row(offset)*2*MAX_COLS))/2;
}
