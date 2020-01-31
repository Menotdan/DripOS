#include "snake.h"
#include <stdlib.h>
#include "../cpu/task.h"

void snake_main() {
    vesa_buffer_t snake_screen = new_framebuffer((width/2), 0, width/2, height);
    swap_display(snake_screen); // Setup display for the game
    set_focused_task(running_task);



    snake_t game_snake;
    apple_t apple;
    apple.x = rand(snake_screen.buffer_width-9);
    apple.y = rand_no_tick(snake_screen.buffer_height-9);


    game_snake.head = kmalloc(sizeof(segment_t));
    game_snake.head->moving_dir = 0;
    game_snake.head->x = 0;
    game_snake.head->y = 0;
    game_snake.score = 1;
    while (1) { // While loop for the game
        fill_screen(0,0,0);

        // Movement handler
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
        // Collision handler
        if (game_snake.head->x < apple.x + 8 &&
        game_snake.head->x + 8 > apple.x &&
        game_snake.head->y < apple.y + 8 &&
        game_snake.head->y + 8 > apple.y) {
            /* Collision */
            play_sound(500+(rand(50)), 100);
            apple.x = rand_no_tick(snake_screen.buffer_width-9);
            apple.y = rand_no_tick(snake_screen.buffer_height-9);
            game_snake.score += 1;
        }
       


        // Keyboard handler
        char scan = get_scancode();
        if (err != 1) {
            if (scancode_to_ascii(scan, 0) == 'q') {
                set_focused_task(get_task_from_pid(1));
                cleanup_framebuffer(current_buffer);
                free(game_snake.head);
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
        rect_fill(apple.x, apple.y, 8, 8, color_from_rgb(255, 0, 0));
        rect_fill(game_snake.head->x, game_snake.head->y, 8, 8, color_from_rgb(255, 255, 255));
        set_cursor_offset(get_offset((snake_screen.text_row/2)-16, snake_screen.text_col+19));
        kprint_no_update("Score: ");
        kprint_uint_no_update(game_snake.score);
        update_display();
        sleep(15);
    }
}