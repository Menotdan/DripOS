#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "../libc/function.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/stdin.h"
#include "../drivers/sound.h"
#include "soundManager.h"
#include <string.h>
#include "../kernel/kernel.h"
#include "task.h"

uint64_t tick = 0; //Ticks
uint64_t prev = 0; //Previous ticks for wait() function
int lSnd = 0; //Length of sound
int task = 0; //is task on?
int pSnd = 0; //ticks before sound started
uint32_t okok = 0;
uint32_t time_slice_left = 1;
//registers_t *temp;
uint32_t switch_task = 0;

static void timer_callback(registers_t *regs) {
    tick++;
    // if (loaded == 1) {
    //     time_slice_left--;
    // }

    // /* Unsleep sleeping processes */
    // Task *iterator = (&main_task)->next;
    // while (iterator->pid != 0) {
    //     /* Set all tasks waiting for this tick to running */
    //     if (iterator->state == SLEEPING) {
    //         if (iterator->waiting == tick) {
    //             iterator->state = RUNNING;
    //         }
    //     }
    //     iterator = iterator->next;
    // }

    // running_task->ticks_cpu_time++;
    // if (time_slice_left == 0 && loaded == 1) {
    //     if (running_task->next->priority == NORMAL) {
    //         time_slice_left = 8; // 16 ms
    //     }
    //     if (running_task->next->priority == HIGH) {
    //         time_slice_left = 30; // 24 ms
    //     }
    //     if (running_task->next->priority == LOW) {
    //         time_slice_left = 8; // 8 ms
    //     }
    //     /* Set the switch task variable, which indicates to the assembly handler
    //     that the next task is ready to be loaded */
    //     switch_task = 1;
    // }

    UNUSED(regs);
}

void config_timer(uint32_t frequency) {
    /* Get the PIT value: maximum hardware clock at 1193180 Hz */
    uint32_t divisor = frequency;

    /* Caluclate the bytes to send to the PIT,
    where the bytes indicate the frequency */
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0xFF);

    /* Send the command to set the frequency */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low); /* Low byte of the frequency */
    port_byte_out(0x40, high); /* High byte of the frequency */
}

void init_timer(uint32_t freq) {
    /* Load the timer callback into IRQ0s handler and configure the timer */
    register_interrupt_handler(IRQ0, timer_callback);
    config_timer(freq);
}

void wait(uint32_t ms) {
    prev = tick;
    while(tick < ms*10 + prev) {
        asm volatile("hlt");
    }
}

void sleep(uint32_t ms) {
    running_task->state = SLEEPING;
    running_task->waiting = ms + tick;
    yield();
}