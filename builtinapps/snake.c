#include "snake.h"
#include <stdlib.h>
#include "../cpu/task.h"

void snake_main() {
    vesa_buffer_t snake_screen = new_framebuffer((width/2), 0, width/2, height);
    swap_display(snake_screen); // Setup display for the game
    set_focused_task(running_task);

    /* snake_t game_snake;
    game_snake.head = kmalloc(sizeof(segment_t));
    game_snake.head->moving_dir = 0;
    game_snake.head->x = 0;
    game_snake.head->y = 0;
    game_snake.score = 1; */
    while (1) { // While loop for the game
        fill_screen(0,0,0);
        /* TODO: Control which tasks get keyboard input */
        char scan = get_scancode();
        if (scancode_to_ascii(scan, 0) == 'q') {
            set_focused_task(get_task_from_pid(1));
            exit();
        }
        if (err != 1) {
            //if (scan == LARROW) {
            //    draw_pixel(0,0,255,255,255);
            //}
        } else {
            err = 0;
        }
        /* game_snake.head->x++;
        game_snake.head->y++;
        draw_pixel(game_snake.head->x, 0, 255,255,255); */
        rand(1234);
        for (uint32_t y = 0; y < current_buffer.buffer_height; y++) {
            for (uint32_t x = 0; x < current_buffer.buffer_width; x++) {
                uint32_t *vidmemcur = (uint32_t *)current_buffer.graphics_vid_buffer;
                uint32_t offset = ((y * current_buffer.buffer_width) + x);
                vidmemcur += offset;
                *vidmemcur = (rand_no_tick(255) << (red_byte)) | (rand_no_tick(255) << (green_byte)) | (rand_no_tick(255) << (blue_byte));
            }
        }
        sprint("\nRandom number: ");
        sprint_uint(rand(1234));
        sprint("\nRandom number: ");
        sprint_uint(rand_no_tick(1234));

        update_display();
    }
}