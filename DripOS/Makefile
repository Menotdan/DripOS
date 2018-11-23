C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o cpu/interrupt.o} 

# Change this if your cross-compiler is somewhere else
CC = /usr/bin/i686-elf-gcc
GDB = /usr/bin/i686-elf-gdb
# -g: Use debugging symbols in gcc
CFLAGS = -g #-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra

# First rule is run by default
os-image.bin: kernel.o boot.o
	i686-elf-gcc -T linker.ld -o os-image.bin -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc

# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case
kernel.o: ${OBJ}
	i686-elf-ld -r -o $@ -Ttext 0x1000 $^ 

# Used for debugging purposes
kernel.elf: boot/kernel_entry.o ${OBJ}
	i686-elf-ld -o $@ -Ttext 0x1000 $^ 

run: myos.iso
	qemu-system-i386 -soundhw pcspk os-image.iso

myos.iso: os-image.bin
	mkdir -p isodir/boot/grub
	cp os-image.bin isodir/boot/os-image.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os-image.iso isodir

iso: myos.iso
	cp os-image.iso .
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