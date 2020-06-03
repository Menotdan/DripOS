#include "urm.h"
#include "scheduler.h"

urm_type_t urm_type;
void *urm_data;
lock_t urm_lock = {0, 0, 0, 0};
event_t urm_request_event = 0;
event_t urm_done_event = 0;

void urm_thread() {
    while (1) {
        await_event(&urm_request_event); // Wait for a URM request
        switch (urm_type) {
            case URM_KILL_PROCESS:
                break;
            case URM_KILL_THREAD:
                break;
            default:
                break;
        }
        trigger_event(&urm_done_event);
    }
}

void send_urm_request(void *data, urm_type_t type) {
    lock(urm_lock);
    urm_data = data;
    urm_type = type;
    trigger_event(&urm_request_event);
    await_event(&urm_done_event);
    unlock(urm_lock);
}