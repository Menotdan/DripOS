#ifndef URM_H
#define URM_H
#include <stdint.h>
#include "event.h"
#include "klibc/lock.h"

typedef enum {
    URM_KILL_PROCESS,
    URM_KILL_THREAD,
    URM_EXECVE,
} urm_type_t;

typedef struct {
    int64_t pid;
} urm_kill_process_data;

typedef struct {
    int64_t tid;
} urm_kill_thread_data;

typedef struct {
    char **argv;
    char **envp;
    int envc;
    int argc;
    char *executable_path;
    int64_t pid;
    int64_t tid;
} urm_execve_data;

void urm_thread();
int send_urm_request(void *data, urm_type_t type);
void send_urm_request_isr(void *data, urm_type_t type);
void send_urm_request_async(void *data, urm_type_t type);

void kill_thread(int64_t tid);

#endif