C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c fs/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h fs/*.h)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o cpu/interrupt.o} 

C_SOURCES_PT2 = $(wildcard kernelPt2/*.c driversPt2/*.c cpuPt2/*.c libcPt2/*.c fsPt2/*.c)
HEADERS_PT2 = $(wildcard kernelPt2/*.h driversPt2/*.h cpuPt2/*.h libcPt2/*.h fsPt2/*.h)
# Nice syntax for file extension replacement
OBJ_PT2 = ${C_SOURCES_PT2:.c=.o cpuPt2/interrupt.o} 

# Change this if your cross-compiler is somewhere else
CC = /usr/bin/i686-elf-gcc
GDB = /usr/bin/i686-elf-gdb
# -g: Use debugging symbols in gcc
CFLAGS = -Og #-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra

# First rule is run by default
os-image.bin: boot/bootsect.bin kernel.bin
	cat $^ > os-image.bin

# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case
kernel.bin: boot/kernel_entry.o ${OBJ}
	i686-elf-ld -o $@ -Ttext 0x1000 $^ --oformat binary

kernel2.bin: boot/kernel_entry.o ${OBJ_PT2}
	i686-elf-ld -o $@ -Ttext 0x4C4B40 $^ --oformat binary
	dd if=/dev/zero of=dripdisk.img bs=1k count=20000
	mkdosfs -F 12 dripdisk.img
	cat $@ >> dripdisk.img


# Used for debugging purposes
kernel.elf: boot/kernel_entry.o ${OBJ}
	i686-elf-ld -o $@ -Ttext 0x1000 $^ 

run: os-image.bin kernel2.bin
	echo "------------NOTE----------------"
	echo "Please select floppy drive as boot drive"
	echo "------------NOTE----------------"
	qemu-system-i386 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -d guest_errors -boot menu=on -fda os-image.bin -hda dripdisk.img

myos.iso: os-image.bin
	dd if=/dev/zero of=floppy.img bs=1024 count=1440
	dd if=os-image.bin of=floppy.img seek=0 conv=notrunc
	cp floppy.img iso/
	genisoimage -quiet -V 'DRIPOS' -input-charset iso8859-1 -o myos.iso -b floppy.img iso/

iso: myos.iso
	cp myos.iso doneiso/
# Open the connection to qemu and load our kernel-object file with symbols
debug: os-image.bin kernel.elf
	qemu-system-i386 -s -fda os-image.bin -d guest_errors,int &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

# Generic rules for wildcards
# To make an object, always compile from its .c
%.o: %.c ${HEADERS}
	i686-elf-gcc ${CFLAGS} -ffreestanding -c $< -o $@

%.o: %.asm
	nasm $< -f elf -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o
