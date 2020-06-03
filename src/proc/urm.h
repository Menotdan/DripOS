#ifndef URM_H
#define URM_H
#include <stdint.h>
#include "event.h"
#include "klibc/lock.h"

typedef enum {
    URM_KILL_PROCESS,
    URM_KILL_THREAD,
} urm_type_t;

typedef struct {
    int64_t pid;
} urm_kill_process_data;

typedef struct {
    int64_t tid;
} urm_kill_thread_data;

void urm_thread();
void send_urm_request(void *data, urm_type_t type);

#endif