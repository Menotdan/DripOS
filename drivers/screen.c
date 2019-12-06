#include "screen.h"
#include "../cpu/ports.h"
#include "../cpu/types.h"
#include <stddef.h>
#include "../libc/mem.h"
#include "colors.h"
#include "../libc/string.h"
#include "../cpu/isr.h"
#include "../cpu/task.h"

/* Declaration of private functions */
int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
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
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
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
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
    set_cursor_offset(prevOffset);
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
        offset = print_char(message[i++], col, row, BLUE_ON_BLACK);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void kprint_at_col(char *message, int col, int row, char color) {
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
        offset = print_char(message[i++], col, row, color);
        /* Compute row/col for next iteration */
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void kprint(char *message) {
    kprint_at(message, -1, -1);
}

void kprint_color(char *message, char color) {
    kprint_at_col(message, -1, -1, color);
}

void crash_screen(registers_t *crash_state, char *msg, uint8_t printReg) {
    asm volatile("cli");
    clear_screen();
    set_cursor_offset(get_offset(35, 0));
    kprint_color("ERROR", 0x0C);
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
        kprint("   ss: ");
        kprint_uint(crash_state->ss);
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
    }
}

//void drip() {
//	kprint_at_blue("#",  32, 0);
//	kprint_at_blue("#",  32, 1);
//	kprint_at_blue("###",  31, 2);
//	kprint_at_blue("###",  31, 3);
//	kprint_at_blue("#######",  29, 4);
//	kprint_at_blue("###########",  27, 5);
//	kprint_at_blue("###############",  25, 6);
//	kprint_at_blue("#################",  24, 7);
//	kprint_at_blue("###################",  23, 8);
//	kprint_at_blue("###################",  23, 9);
//	kprint_at_blue("#################",  24, 10);
//	kprint_at_blue("###############",  25, 11);
//	kprint_at_blue("###########",  27, 12);
//	kprint_at_blue("DripOS", 30, 14);
//}

void logoDraw() {
    int xOff = 24;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if(logo[y][x] == 1) {
                kprint_at_col(" ", x + xOff, y, BLACK_ON_BLACK);
            } else if(logo[y][x] == 2) {                kprint_at_col(" ", x + xOff, y, CYAN_ON_CYAN);
            } else if(logo[y][x] == 3) {
                kprint_at_col(" ", x + xOff, y, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_WHITE));
            } else {
                kprint_at_col(" ", x + xOff, y, WHITE_ON_BLACK);
            }
        }
    }
}

void kprint_backspace() {
    int offset = get_cursor_offset()-2;
    int row = get_offset_row(offset);
    int col = get_offset_col(offset);
    print_char(0x08, col, row, WHITE_ON_BLACK);
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
int print_char(char c, int col, int row, char attr) {
    uint8_t *vidmem = (uint8_t*) VIDEO_ADDRESS;
    if (!attr) attr = WHITE_ON_BLACK;

    /* Error control: print a red 'E' if the coords aren't right */
    if (col >= MAX_COLS || row >= MAX_ROWS) {
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-2] = 'E';
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-1] = RED_ON_WHITE;
        return get_offset(col, row);
    }

    int offset;
    if (col >= 0 && row >= 0) offset = get_offset(col, row);
    else offset = get_cursor_offset();

    if (c == '\n') {
        row = get_offset_row(offset);
        offset = get_offset(0, row+1);
    } else if (c == 0x08) { /* Backspace */
        vidmem[offset] = ' ';
        vidmem[offset+1] = attr;
    } else {
        vidmem[offset] = c;
        vidmem[offset+1] = attr;
        offset += 2;
    }

    /* Check if the offset is over screen size and scroll */
    if (offset >= MAX_ROWS * MAX_COLS * 2) {
        int i;
        for (i = 1; i < MAX_ROWS; i++) 
            memory_copy((uint8_t*)(get_offset(0, i) + VIDEO_ADDRESS),
                        (uint8_t*)(get_offset(0, i-1) + VIDEO_ADDRESS),
                        MAX_COLS * 2);

        /* Blank last line */
        char *last_line = (char*) (get_offset(0, MAX_ROWS-1) + (uint8_t*) VIDEO_ADDRESS);
        for (i = 0; i < MAX_COLS * 2; i++) last_line[i] = 0;

        offset -= 2 * MAX_COLS;
    }

    set_cursor_offset(offset);
    return offset;
}

int get_cursor_offset() {
    /* Use the VGA ports to get the current cursor position
     * 1. Ask for high byte of the cursor offset (data 14)
     * 2. Ask for low byte (data 15)
     */
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8; /* High byte: << 8 */
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2; /* Position * size of character cell */
}

void set_cursor_offset(int offset) {
    /* Similar to get_cursor_offset, but instead of reading we write data */
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (uint8_t)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (uint8_t)(offset & 0xff));
}

void clear_screen() {
    int screen_size = MAX_COLS * MAX_ROWS;
    int i;
    uint8_t *screen = (uint8_t*) VIDEO_ADDRESS;

    for (i = 0; i < screen_size; i++) {
        screen[i*2] = ' ';
        screen[i*2+1] = WHITE_ON_BLACK;
    }
    set_cursor_offset(get_offset(0, 0));
}


int get_offset(int col, int row) { return 2 * (row * MAX_COLS + col); }
int get_offset_row(int offset) { return offset / (2 * MAX_COLS); }
int get_offset_col(int offset) { return (offset - (get_offset_row(offset)*2*MAX_COLS))/2; }
