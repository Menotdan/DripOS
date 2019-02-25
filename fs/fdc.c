#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "../libc/function.h"
#include "fdc.h"
#include "../cpu/timer.h"
#include "../drivers/screen.h"
#define DMA_BUFFER 0x1000
int floppy = 0;
//Floppy handler
void floppy_handler(registers_t regs) {
    floppy = 1;
    kprint("floppy irq");
    UNUSED(regs);
}
//Register the floppy handler
void init_floppy() {
    register_interrupt_handler(IRQ6, floppy_handler);
    kprint("Floppy registered");
}
void flpydsk_wait_irq() {
    kprint("started waiting");
    for (int h=0; h<100; h++) {
        if (floppy == 1) {
            floppy = 0;
            kprint("found irq");
            return;
        }
    }
    kprint("failed");
    return;
}
//! initialize DMA to use phys addr 1k-64k
void flpydsk_initialize_dma () {
 
	port_byte_out (0x0a,0x06);	//mask dma channel 2
	port_byte_out (0xd8,0xff);	//reset master flip-flop
	port_byte_out (0x04, 0);     //address=0x1000 
	port_byte_out (0x04, 0x10);
	port_byte_out (0xd8, 0xff);  //reset master flip-flop
	port_byte_out (0x05, 0xff);  //count to 0x23ff (number of bytes in a 3.5" floppy disk track)
	port_byte_out (0x05, 0x23);
	port_byte_out (0x80, 0);     //external page register = 0
	port_byte_out (0x0a, 0x02);  //unmask dma channel 2
}
 
//! prepare the DMA for read transfer
void flpydsk_dma_read () {
 
	port_byte_out (0x0a, 0x06); //mask dma channel 2
	port_byte_out (0x0b, 0x56); //single transfer, address increment, autoinit, read, channel 2
	port_byte_out (0x0a, 0x02); //unmask dma channel 2
}
 
//! prepare the DMA for write transfer
void flpydsk_dma_write () {
 
	port_byte_out (0x0a, 0x06); //mask dma channel 2
	port_byte_out (0x0b, 0x5a); //single transfer, address increment, autoinit, write, channel 2
	port_byte_out (0x0a, 0x02); //unmask dma channel 2
}

void flpydsk_write_dor (uint8_t val ) {
	//! write the digital output register
	port_byte_out (FLPYDSK_DOR, val);
}

void flpydsk_disable_controller () {
 
	flpydsk_write_dor (0);
}

void flpydsk_enable_controller () {
 
	flpydsk_write_dor ( FLPYDSK_DOR_MASK_RESET | FLPYDSK_DOR_MASK_DMA);
}


void start_motor() {
    //! Start the floppy drive motor
    port_byte_out (FLPYDSK_DOR, FLPYDSK_DOR_MASK_DRIVE0_MOTOR | FLPYDSK_DOR_MASK_RESET);
}

void stop_motor() {
    //! Stop the floppy drive motor
    port_byte_out (FLPYDSK_DOR, FLPYDSK_DOR_MASK_RESET);
}

uint8_t flpydsk_read_status () {
 
	//! just return main status register
	return port_byte_in (FLPYDSK_MSR);
}

void flpydsk_send_command (uint8_t cmd) {
 
	//! wait until data register is ready. We send commands to the data register
	for (int i = 0; i < 500; i++ )
		if ( flpydsk_read_status () & FLPYDSK_MSR_MASK_DATAREG )
			return port_byte_out (FLPYDSK_FIFO, cmd);
}
 
uint8_t flpydsk_read_data () {
 
	//! same as above function but returns data register for reading
	for (int i = 0; i < 500; i++ )
		if ( flpydsk_read_status () & FLPYDSK_MSR_MASK_DATAREG )
			return port_byte_in (FLPYDSK_FIFO);
}

void flpydsk_check_int (uint32_t* st0, uint32_t* cyl) {
 
	flpydsk_send_command (FDC_CMD_CHECK_INT);
 
	*st0 = flpydsk_read_data ();
	*cyl = flpydsk_read_data ();
}

void flpydsk_write_ccr (uint8_t val) {
 
	//! write the configuation control
	port_byte_out (FLPYDSK_CTRL, val);
}

void flpydsk_lba_to_chs (int lba,int *head,int *track,int *sector) {
 
   *head = ( lba % ( 80 * 2 ) ) / ( 80 );
   *track = lba / ( 80 * 2 );
   *sector = lba % 80 + 1;
}

void flpydsk_read_sector_imp (uint8_t head, uint8_t track, uint8_t sector) {
    kprint("imp called");
	uint32_t st0, cyl;
    kprint("dmaR");
	//! set the DMA for read transfer
	flpydsk_dma_read ();
    kprint("sending commands");
	//! read in a sector
	flpydsk_send_command (
		FDC_CMD_READ_SECT | FDC_CMD_EXT_MULTITRACK |
		FDC_CMD_EXT_SKIP | FDC_CMD_EXT_DENSITY);
	flpydsk_send_command ( head << 2 | _CurrentDrive );
	flpydsk_send_command ( track);
	flpydsk_send_command ( head);
	flpydsk_send_command ( sector);
	flpydsk_send_command ( FLPYDSK_SECTOR_DTL_512 );
	flpydsk_send_command (
		( ( sector + 1 ) >= 18 )
			? 18 : sector + 1 );
	flpydsk_send_command ( FLPYDSK_GAP3_LENGTH_3_5 );
	flpydsk_send_command ( 0xff );
    kprint("irq wait");
	//! wait for irq
	flpydsk_wait_irq ();
    kprint("read bytes");
	//! read status info
	for (int j=0; j<7; j++)
		flpydsk_read_data();
    kprint("tell fdc that we handled the interrupts");
	//! let FDC know we handled interrupt
	flpydsk_check_int (&st0,&cyl);
}

uint8_t* flpydsk_read_sector (int sectorLBA) {
    kprint("called");
	if (_CurrentDrive >= 4)
		return 0;
 
	//! convert LBA sector to CHS
    kprint("converting");
	int head=0, track=0, sector=1;
	flpydsk_lba_to_chs (sectorLBA, &head, &track, &sector);
    kprint("starting motor");
	//! turn motor on and seek to track
	
    kprint("verifying position");
	if (flpydsk_seek (track, head) != 0)
		return 0;
    kprint("imp read");
	//! read sector and turn motor off
	flpydsk_read_sector_imp (head, track, sector);
    kprint("stopping motor");
	stop_motor();
    kprint("returning buffer");
	//! warning: this is a bit hackish
	return (uint8_t*) DMA_BUFFER;
}

int flpydsk_calibrate (uint32_t drive) {
 
	uint32_t st0, cyl;
 
	if (drive >= 4)
		return -2;
 
	//! turn on the motor
	start_motor();
 
	for (int i = 0; i < 10; i++) {
 
		//! send command
		flpydsk_send_command ( FDC_CMD_CALIBRATE );
		flpydsk_send_command ( drive );
		flpydsk_wait_irq ();
		flpydsk_check_int ( &st0, &cyl);
 
		//! did we fine cylinder 0? if so, we are done
		if (!cyl) {
 
			stop_motor();
			return 0;
		}
	}
 
	stop_motor();
	return -1;
}

int flpydsk_seek ( uint32_t cyl, uint32_t head ) {
 
	uint32_t st0, cyl0;
 
	if (_CurrentDrive >= 4)
		return -1;
 
	for (int i = 0; i < 10; i++ ) {
 
		//! send the command
		flpydsk_send_command (FDC_CMD_SEEK);
		flpydsk_send_command ( (head) << 2 | _CurrentDrive);
		flpydsk_send_command (cyl);
 
		//! wait for the results phase IRQ
		flpydsk_wait_irq ();
		flpydsk_check_int (&st0,&cyl0);
 
		//! found the cylinder?
		if ( cyl0 == cyl)
			return 0;
	}
 
	return -1;
}

void flpydsk_drive_data (uint32_t stepr, uint32_t loadt, uint32_t unloadt, bool dma ) {
 
	uint32_t data = 0;
 
	flpydsk_send_command (FDC_CMD_SPECIFY);
 
	data = ( (stepr & 0xf) << 4) | (unloadt & 0xf);
	flpydsk_send_command (data);
 
	data = (loadt) << 1 | (dma==true) ? 1 : 0;
	flpydsk_send_command (data);
}


void flpydsk_reset () {
 
	uint32_t st0, cyl;
 
	//! reset the controller
	flpydsk_disable_controller ();
	flpydsk_enable_controller ();
	flpydsk_wait_irq ();
 
	//! send CHECK_INT/SENSE INTERRUPT command to all drives
	for (int i=0; i<4; i++)
		flpydsk_check_int (&st0,&cyl);
 
	//! transfer speed 500kb/s
	flpydsk_write_ccr (0);
 
	//! pass mechanical drive info. steprate=3ms, unload time=240ms, load time=16ms
	flpydsk_drive_data (3,16,240,true);
 
	//! calibrate the disk
	flpydsk_calibrate ( _CurrentDrive );
}
