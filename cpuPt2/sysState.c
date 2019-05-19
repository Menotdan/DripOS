#include "sysState.h"
#include "isr.h"
#include "../libcPt2/function.h"
#include "../driversPt2/screen.h"
#include "../kernelPt2/kernel.h"
#include "ports.h"
#include "timer.h"
#include "../fsPt2/hddw.h"

void sys_state_manager() {
    int state = getstate();
	if (state == 0) {
		//System is in a normal state(as far as the kernel is aware); Nothing to do
	} else if (state == 1) {
		//User has called shutdown; Time to shutdown
		//Shutdown processes safely
		//halt cpu
		//halt();
		port_byte_out(0xf4, 0x00);
	} else if (state == 2) {
		//Uh Oh!!!! Kernel Panic!!!! Display Error then HALT the CPU
		//0x7263
		writeIn[0] = 0x7263;
		writeFromBuffer(128);
		port_byte_out(0xf4, 0x00);
	}
}