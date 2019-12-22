#include "snake.h"

void snake_main() {
    vesa_tty_t snake_screen = new_framebuffer((width/2)+1, 0, width/2, height);
    uint32_t x = 0;
    while (1) {
        vesa_tty_t cur_scr = swap_display(snake_screen);
        char scan = get_scancode();
        if (err != 1) {
            if (scan == LARROW) {
                draw_pixel(x,0,255,255,0);
                x++;
                update_display();
            }
        } else {
            err = 0;
        }
        swap_display(cur_scr);
    }
}