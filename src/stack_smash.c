#include <stdint.h>
#include "klibc/stdlib.h"
#include "klibc/string.h"
 
#define STACK_CHK_GUARD 0x595e9fbd94fda766
uint64_t __stack_chk_guard = STACK_CHK_GUARD;
 
void __stack_chk_fail() {
    char panic_msg[200] = "Stack smashing detected! Addr: ";
    char addr[32];

    htoa((uint64_t) __builtin_return_address(0), addr);
    strcat(panic_msg, addr);

    panic(panic_msg);
}