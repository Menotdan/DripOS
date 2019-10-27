#include <debug.h>
#include "../drivers/screen.h"
void breakA() {
    // Debugger break 
}

void asserta(int condition, char *msg){
    if(!condition){
        kprint(msg);
        asm volatile("int $0x14");
        //asm volatile("hlt");
    }
}