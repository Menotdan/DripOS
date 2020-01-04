#include "snake.h"
#include <stdlib.h>
#include "../cpu/task.h"

void snake_main() {
    vesa_buffer_t snake_screen = new_framebuffer((width/2), 0, width/2, height);
    swap_display(snake_screen); // Setup display for the game
    set_focused_task(running_task);
    uint32_t time = 0;

    snake_t game_snake;
    game_snake.head = kmalloc(sizeof(segment_t));
    game_snake.head->moving_dir = 0;
    game_snake.head->x = 0;
    game_snake.head->y = 0;
    game_snake.score = 1;
    time = tick;
    while (1) { // While loop for the game
        fill_screen(0,0,0);
        /* TODO: Control which tasks get keyboard input */
        char scan = get_scancode();
        // Keyboard handler
        if (game_snake.head->moving_dir == 0) {
            if (game_snake.head->x < (snake_screen.buffer_width)) {
                game_snake.head->x++;
            } else if (game_snake.head->x == (snake_screen.buffer_width)) {
                if (game_snake.head->y < snake_screen.buffer_height) {
                    game_snake.head->y += 1;
                    game_snake.head->x = 0;
                    sprint("\nCleared 1 row (320 pixels) in ");
                    sprint_uint(tick-time);
                    sprint(" ms");
                    sprint("\nFPS: ");
                    sprint_uint((320000 / (tick-time)));
                    time = tick;
                } else {
                    game_snake.head->x = 0;
                    game_snake.head->y = 0;
                }
            }
        }
        if (err != 1) {
            sprint("\nGot scancode");
            if (scancode_to_ascii(scan, 0) == 'q') {
                set_focused_task(get_task_from_pid(1));
                cleanup_framebuffer(current_buffer);
                exit();
            } else if (scan == LARROW) {
                
            }
        } else {
            err = 0;
        }
        draw_pixel(game_snake.head->x,game_snake.head->y,255,255,255);
        update_display();
        //sleep(15);
    }
}