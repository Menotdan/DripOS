#pragma once

#include <stdint.h>
#include <string.h>
#include <function.h>
#include <drivers/screen.h>
#include <drivers/keyboard.h>
#include <drivers/stdin.h>
#include <drivers/sound.h>
#include <kernel/systemManager.h>
#include <kernel/kernel.h>
#include "isr.h"
#include "ports.h"
#include "sysState.h"
#include "soundManager.h"
//#include "task.h"

void init_timer(uint32_t freq);
void wait(uint32_t ms);
void sleep(uint32_t ms);

uint32_t tick;
int lSnd;
int pSnd;
int task;
uint32_t switch_task;
uint32_t time_slice_left;