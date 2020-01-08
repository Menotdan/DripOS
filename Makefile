# Cleaned and fixed by StackOverflow user Michael Petch
C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c fs/*.c builtinapps/*.c mem/*.c)
NASM_SOURCES = $(wildcard kernel/*.asm drivers/*.asm cpu/*.asm libc/*.asm fs/*.asm mem/*.asm long_load.asm)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h fs/*.h mem/*.h builtinapps/*.h)
BIG_S_SOURCES = $(wildcard kernel/*.S drivers/*.S cpu/*.S libc/*.S fs/*.S mem/*.S *.S)
S_SOURCES = $(wildcard kernel/*.s drivers/*.s cpu/*.s libc/*.s fs/*.s mem/*.s *.s)
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o}  ${NASM_SOURCES:.asm=.o} ${S_SOURCES:.s=.o} ${BIG_S_SOURCES:.S=.o}
 
# Change this if your cross-compiler is somewhere else
CC = gcc
LINKER = ld
32BITLINKER = i686-elf-ld
incPath = ~/DripOS/include
GDB = gdb
MEM = 1G # Memory for qemu
O_LEVEL = 3
# -g: Use debugging symbols in gcc
CFLAGS = -g -fno-pic               \
	-z max-page-size=0x1000        \
	-mno-sse                       \
	-mno-sse2                      \
	-mno-mmx                       \
	-mno-80387                     \
	-mno-red-zone                  \
	-mcmodel=kernel                \
	-ffreestanding                 \
	-fno-stack-protector           \
    -fno-omit-frame-pointer

# First rule is run by default
myos.iso: kernel32-withboot.elf
	grub-file --is-x86-multiboot kernel32-withboot.elf
	mkdir -p isodir/boot/grub
	cp kernel32-withboot.elf isodir/boot/os-image.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o DripOS.iso isodir
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o fs/*.o mem/*.o
	rm -rf *.bin *.o *.elf

 
# '--oformat binary' deletes all symbols as a collateral, so we don't need
# to 'strip' them manually on this case
kernel.bin: kernel32-withboot.elf
	objcopy -O binary $^ $@

kernel32-withboot.elf: kernel32.elf boot.o
	${32BITLINKER} -o $@ -T linker.ld $^

kernel32.elf: kernel.elf
	objcopy -O elf32-i386 kernel.elf kernel32.elf
 
# Used for debugging purposes
kernel.elf: ${OBJ}
	${LINKER} -o $@ -T linker.ld $^

lol: ${OBJ}
	~/Desktop/Compiler/bin/i686-elf-ld -melf_i386 -o helllo -T linker.ld hello $^

run: myos.iso
	qemu-system-x86_64 -d guest_errors int -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -cdrom DripOS.iso -hda dripdisk.img

run-kvm: myos.iso
	sudo qemu-system-x86_64 -enable-kvm -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -cdrom DripOS.iso -hda dripdisk.img

iso: myos.iso
	cp myos.iso doneiso/
# Open the connection to qemu and load our kernel-object file with symbols
debug: myos.iso
	qemu-system-x86_64 -vga std -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -s -S -boot menu=on -cdrom DripOS.iso -hda dripdisk.img &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf" -x "~/gdbcommands"
 
# Generic rules for wildcards
# To make an object, always compile from its .c $< $@
cpu/switch.o: cpu/switch.s
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@

%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -Iinclude -I. -O${O_LEVEL} -Werror -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -MD -c $< -o $@ -std=gnu11 -ffreestanding
 
%.o: %.s
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@
%.o: %.S
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@

boot.o: boot.asm
	nasm -g -f elf32 -o $@ $<

long_load.o: long_load.asm
	nasm -g -f elf64 -o $@ $<

cpu/interrupt.o: cpu/interrupt.asm
	nasm -g -f elf64 -o $@ $<

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf *.iso
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o fs/*.o mem/*.o
