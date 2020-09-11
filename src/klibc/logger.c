#include "logger.h"
#include "klibc/lock.h"
#include <stdarg.h>

lock_t log_lock = {0, 0, 0, 0};

void log(char *message, ...) {
#ifndef NO_LOG
    lock(log_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    sprint("\e[34m[LOG]\e[32m ");

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index], COM1);
        }
        index++;
    }

    va_end(format_list);
    sprint("\e[0m\n");
    unlock(log_lock);
#else
    (void) message;
#endif
}

void warn(char *message, ...) {
#ifndef NO_LOG
    lock(log_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    sprint("\e[36m[WARN]\e[33m ");

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index], COM1);
        }
        index++;
    }

    va_end(format_list);
    sprint("\e[0m\n");
    unlock(log_lock);
#else
    (void) message;
#endif
}

void error(char *message, ...) {
#ifndef NO_LOG
    lock(log_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);

    sprint("\e[91m[ERROR]\e[31m ");

    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index], COM1);
        }
        index++;
    }

    va_end(format_list);
    sprint("\e[0m\n");
    unlock(log_lock);
#else
    (void) message;
#endif
}

void log_alloc(char *message, ...) {
#ifdef LOG_MEM
    lock(log_lock);
    va_list format_list;
    uint64_t index = 0;
    uint8_t big = 0;

    va_start(format_list, message);
    while (message[index]) {
        if (message[index] == '%') {
            index++;
            if (message[index] == 'l') {
                index++;
                big = 1;
            }
            switch (message[index]) {
                case 'x':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        htoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        htoa((uint64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'd':
                    if (big) {
                        int64_t data = va_arg(format_list, int64_t);
                        char data_buf[32];
                        itoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        int32_t data = va_arg(format_list, int32_t);
                        char data_buf[32];
                        itoa((int64_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 'u':
                    if (big) {
                        uint64_t data = va_arg(format_list, uint64_t);
                        char data_buf[32];
                        utoa(data, data_buf);
                        sprint(data_buf);
                    } else {
                        uint32_t data = va_arg(format_list, uint32_t);
                        char data_buf[32];
                        utoa((uint32_t) data, data_buf);
                        sprint(data_buf);
                    }
                    break;
                case 's':
                    sprint(va_arg(format_list, char *));
                    break;
                default:
                    break;
            }
        } else {
            write_serial(message[index], COM1);
        }
        index++;
    }
    va_end(format_list);
    unlock(log_lock);
#else
    (void) message;
#endif
}