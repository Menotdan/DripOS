#include "snake.h"
#include <stdlib.h>
#include "../cpu/task.h"

void snake_main() {
    vesa_buffer_t snake_screen = new_framebuffer((width/2), 0, width/2, height);
    swap_display(snake_screen); // Setup display for the game
    set_focused_task(running_task);



    snake_t game_snake;
    game_snake.head = kmalloc(sizeof(segment_t));
    game_snake.head->moving_dir = 0;
    game_snake.head->x = 0;
    game_snake.head->y = 0;
    game_snake.score = 1;
    while (1) { // While loop for the game
        fill_screen(0,0,0);
        /* TODO: Control which tasks get keyboard input */
        char scan = get_scancode();
        // Keyboard handler
        if (game_snake.head->moving_dir == 0) {
            if (game_snake.head->x < (snake_screen.buffer_width-9)) {
                game_snake.head->x++;
            }
        } else if (game_snake.head->moving_dir == 1) {
            if (game_snake.head->x > 0) {
                game_snake.head->x--;
            }
        } else if (game_snake.head->moving_dir == 2) {
            if (game_snake.head->y < (snake_screen.buffer_height-9)) {
                game_snake.head->y++;
            }
        } else if (game_snake.head->moving_dir == 3) {
            if (game_snake.head->y > 0) {
                game_snake.head->y--;
            }
        } 
        if (err != 1) {
            if (scancode_to_ascii(scan, 0) == 'q') {
                set_focused_task(get_task_from_pid(1));
                cleanup_framebuffer(current_buffer);
                free(game_snake.head, sizeof(segment_t));
                exit();
            } else if (scan == LARROW) {
                game_snake.head->moving_dir = 1;
            } else if (scan == RARROW) {
                game_snake.head->moving_dir = 0;
            } else if (scan == UPARROW) {
                game_snake.head->moving_dir = 3;
            } else if (scan == DOWNARROW) {
                game_snake.head->moving_dir = 2;
            }
        } else {
            err = 0;
        }
        rect_fill(game_snake.head->x, game_snake.head->y, 8, 8, color_from_rgb(255, 255, 255));
        update_display();
        sleep(15);
    }
}