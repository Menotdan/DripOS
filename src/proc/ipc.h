#ifndef IPC_H
#define IPC_H
#include "event.h"
#include "klibc/lock.h"

#define IPC_OPERATION_WRITE 1
#define IPC_OPERATION_READ 0

#define IPC_CONNECT_TIMEOUT_MS 50

#define IPC_OPERATION_NOT_SUPPORTED 5
#define IPC_BUFFER_INVALID 4
#define IPC_BUFFER_TOO_SMALL 3
#define IPC_CONNECTION_TIMEOUT 2
#define IPC_NOT_LISTENING 1
#define IPC_SUCCESS 0
typedef struct {
    int pid;
    void *buffer;
    int size;
    int operation_type; // Read vs Write
    int listening; // Is the server listening?
    int err; // error for the server to return
    lock_t connect_lock; // The lock for connecting to the server for an operation
    event_t *ipc_event; // This event is what IPC clients should trigger to signal ready to transfer
    event_t *ipc_completed; // This event will be triggered when the IPC server is done its work
} ipc_handle_t;

typedef struct {
    uint32_t err;
    uint32_t size;
} __attribute__((packed)) ipc_error_info_t;

union ipc_err {
    uint64_t real_err;
    ipc_error_info_t parts;
};

union ipc_err read_ipc_server(int pid, int port, void *buf, int size);
union ipc_err write_ipc_server(int pid, int port, void *buf, int size);
int register_ipc_handle(int port);
ipc_handle_t *wait_ipc(int port);

void setup_ipc_servers();

#endif