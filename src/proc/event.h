#ifndef EVENT_H
#define EVENT_H
#include <stdint.h>

typedef int event_t;

void await_event(event_t *e);
void await_event_timeout(event_t *e, uint64_t timeout);
void trigger_event(event_t *e);

#endif