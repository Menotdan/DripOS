# Cleaned and fixed by StackOverflow user Michael Petch
C_SOURCES = $(shell find src/ -type f -name '*.c')
NASM_SOURCES = $(shell find src/ -type f -name '*.asm')
REAL_SOURCES = $(shell find src/ -type f -name '*.real')
# Nice syntax for file extension replacement
OBJ = ${C_SOURCES:.c=.o} ${NASM_SOURCES:.asm=.o}
 
# Change this if your cross-compiler is somewhere else

CC = x86_64-elf-gcc
LINKER = x86_64-elf-ld

incPath = ~/DripOS/src
GDB = gdb
MEM = 2G # Memory for qemu
CORES = 4
O_LEVEL = 2 # Optimization level
# Options for GCC
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
myos.iso: kernel.elf
	grub-file --is-x86-multiboot kernel.elf
	mkdir -p build_iso/boot/grub
	cp kernel.elf build_iso/boot/os-image.bin
	cp grub.cfg build_iso/boot/grub/grub.cfg
	grub-mkrescue -o DripOS.iso build_iso
	rm -rf build_iso

kernel.elf: ${NASM_SOURCES:.real=.bin} ${OBJ}
	${CC} -Wl,-z,max-page-size=0x1000 -nostdlib -o $@ -T linker.ld ${OBJ}

run: myos.iso
	- qemu-system-x86_64 -d guest_errors,cpu_reset -smp ${CORES} -machine q35 -no-shutdown -no-reboot -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -cdrom DripOS.iso -hda dripdisk.img
	make clean

run-kvm: myos.iso
	- sudo qemu-system-x86_64 -machine q35 -enable-kvm -smp ${CORES} -cpu host -d cpu_reset -no-shutdown -no-reboot -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -cdrom DripOS.iso -hda dripdisk.img
	make clean

# Open the connection to qemu and load our kernel-object file with symbols
debug: myos.iso
	qemu-system-x86_64 -smp ${CORES} -vga std -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -s -S -boot menu=on -cdrom DripOS.iso -hda dripdisk.img &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"
	make clean
 
# Generic rules for wildcards
# To make an object, always compile from its .c

%.o: %.c
	${CC} ${CFLAGS} -D AMD64 -D DEBUG -Iinclude -I src -O${O_LEVEL} -Werror -Wall -Wextra -fno-omit-frame-pointer -MD -c $< -o $@ -std=gnu11 -ffreestanding

%.bin: %.real
	nasm -f bin -o $@ $<

%.o: %.s
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@
%.o: %.S
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@

%.o: %.asm
	nasm -f elf64 -o $@ $<

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf *.iso
	rm -rf ${OBJ}
	rm -rf $(shell find src/ -type f -name '*.d')