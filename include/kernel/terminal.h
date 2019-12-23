#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <mem.h>
#include <drivers/sound.h>
#include <drivers/time.h>
#include <drivers/screen.h>
#include <drivers/stdin.h>
#include <drivers/vesa.h>
#include <drivers/serial.h>
#include <fs/hdd.h>
#include <fs/hddw.h>
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include "../../cpu/soundManager.h"
#include "../../cpu/timer.h"
#include "../../cpu/task.h"
#include "../../builtinapps/snake.h"

//lol
void execute_command(char input[]);
void init_terminal();