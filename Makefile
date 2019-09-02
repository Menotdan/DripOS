C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c fs/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h fs/*.h)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o} 

# Change this if your cross-compiler is somewhere else
CC = /usr/bin/i686-elf-gcc
GDB = gdb
# -g: Use debugging symbols in gcc
CFLAGS = -g #-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra

# First rule is run by default
os-image.bin: boot.o kernel.bin
	i686-elf-gcc -T linker.ld -o os-image.bin -ffreestanding -O2 -nostdlib boot.o ${OBJ} cpu/interrupt.o -lgcc
	grub-file --is-x86-multiboot os-image.bin
	mkdir -p isodir/boot/grub
	cp os-image.bin isodir/boot/os-image.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o DripOS.iso isodir

# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case
kernel.bin: ${OBJ} cpu/interrupt.o
	i686-elf-ld -melf_i386 -o kernel.elf ${OBJ} cpu/interrupt.o
# Used for debugging purposes
kernel.elf: boot/kernel_entry.o ${OBJ}
	i686-elf-ld -o $@ -Ttext 0x1000 $^ 

run: os-image.bin
	echo "------------NOTE----------------"
	echo "Please select floppy drive as boot drive"
	echo "------------NOTE----------------"
	qemu-system-x86_64 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -cdrom DripOS.iso -hdb dripdisk.img

myos.iso: os-image.bin
	dd if=/dev/zero of=floppy.img bs=1024 count=1440
	dd if=os-image.bin of=floppy.img seek=0 conv=notrunc
	cp floppy.img iso/
	genisoimage -quiet -V 'DRIPOS' -input-charset iso8859-1 -o myos.iso -b floppy.img iso/

iso: myos.iso
	cp myos.iso doneiso/
# Open the connection to qemu and load our kernel-object file with symbols
debug: os-image.bin
	qemu-system-x86_64 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -s -S -boot menu=on -fda boot/bootsect.bin -hdb dripdisk.img -hda kernel.bin &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file bootsect.elf"

# Generic rules for wildcards
# To make an object, always compile from its .c $< $@
%.o: %.c ${HEADERS}
	i686-elf-gcc -O2 -g -MD -c $< -o $@ -std=gnu11

boot.o: boot.s
	i686-elf-gcc -O2 -g -MD -c $< -o $@

cpu/interrupt.o:
	nasm -f elf cpu/interrupt.asm

%.bin: %.asm
	nasm -g -f elf32 -F dwarf -o bootsect.o $<
	ld -melf_i386 -Ttext=0x7c00 -nostdlib --nmagic -o bootsect.elf bootsect.o
	objcopy -O binary bootsect.elf $@

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf *.iso
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o
