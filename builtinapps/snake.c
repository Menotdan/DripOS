#include "snake.h"

void snake_main() {
    vesa_buffer_t snake_screen = new_framebuffer((width/2)+1, 0, width/2, height);
    swap_display(snake_screen); // Setup display for the game
    while (1) { // While loop for the game
        fill_screen(0,0,0);
        /* TODO: Control which tasks get keyboard input */
        char scan = get_scancode();
        if (err != 1) {
            if (scan == LARROW) {
                draw_pixel(0,0,255,255,255);
            }
        } else {
            err = 0;
        }
        update_display();
    }
}