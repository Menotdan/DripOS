#ifndef SLEEP_QUEUE_H
#define SLEEP_QUEUE_H
#include <stdint.h>

struct timespec {
    uint64_t seconds;
    uint64_t nanoseconds;
};

typedef struct sleep_queue {
    struct sleep_queue *next;
    struct sleep_queue *prev;
    uint64_t time_left;
    int64_t tid;
} sleep_queue_t;

void advance_time();
void sleep_ms(uint64_t ms);

int nanosleep(struct timespec *req, struct timespec *rem);

#endif