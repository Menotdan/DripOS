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

MEM = 1G # Memory for qemu
CORES = 3 # Core count for qemu

O_LEVEL = 2 # Optimization level for build

BOOT_IMAGE_MB=10
BOOT_IMAGE_OUTPUT=/dev/null # safety

IMG_MB=200
DRIPOS_SYSROOT_ON_HOST=/var/dripos-sysroot
IMG_OUTPUT=dripdisk.img
OLD_WD = $(shell pwd)


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
    -fno-omit-frame-pointer        \
	-fstack-protector-all          \
	-D DEBUG

# First rule is run by default
DripOS.img: kernel.elf
	# Create blank image
	dd if=/dev/zero of=DripOS.img bs=1M count=$(BOOT_IMAGE_MB)

	# Create partition tables
	parted -s DripOS.img mklabel msdos
	parted -s DripOS.img mkpart primary 1 100%

	# Format with echfs and import files
	echfs-utils -m -p0 DripOS.img format 512
	echfs-utils -m -p0 DripOS.img import kernel.elf kernel.elf
	echfs-utils -m -p0 DripOS.img import qloader2.cfg qloader2.cfg

	# Install qloader2
	cd qloader2 && ./qloader2-install qloader2.bin ../DripOS.img

export_dripos_img: DripOS.img
	sudo dd if=DripOS.img of=$(BOOT_IMAGE_OUTPUT) bs=1M count=$(BOOT_IMAGE_MB)

disk_image:
	echo "Are you sure you want to write to $(IMG_OUTPUT)?"
	read test
	dd if=/dev/zero of=temp-image bs=1M count=$(IMG_MB)
	echfs-utils temp-image quick-format 512

	- rm -rf mountpoint
	./copy-sysroot-to-echfs-img.sh /var/dripos-sysroot temp-image
	- rm -rf mountpoint

	#./copy-sysroot-to-echfs-img.sh $(DRIPOS_SYSROOT_ON_HOST) temp-image

	sudo dd if=temp-image of=$(IMG_OUTPUT) bs=1M count=$(IMG_MB)
	rm -rf temp-image

kernel.elf: ${NASM_SOURCES:.real=.bin} ${OBJ}
	${CC} -Wl,-z,max-page-size=0x1000,--gc-sections -nostdlib -Werror -Wall -Wextra -Wpedantic -Wunused-function -o $@ -T linker.ld ${OBJ}

run: DripOS.img
	- qemu-system-x86_64 -smp ${CORES} -machine q35 -no-shutdown -no-reboot -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -hdb DripOS.img -hda dripdisk.img
	make clean

run-dripdbg: DripOS.img
	- qemu-system-x86_64 -d guest_errors -smp ${CORES} -machine q35 -no-shutdown -no-reboot -chardev socket,id=char0,port=12345,host=127.0.0.1 -serial chardev:char0 -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -hdb DripOS.img -hda dripdisk.img
	make clean

run-kvm: DripOS.img
	- sudo qemu-system-x86_64 -machine q35 -enable-kvm -smp ${CORES} -cpu host -d cpu_reset -no-shutdown -no-reboot -serial stdio -soundhw hda -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -boot menu=on -hdb DripOS.img -hda dripdisk.img
	make clean

# Open the connection to qemu and load our kernel-object file with symbols
debug: DripOS.img
	sudo qemu-system-x86_64 -machine q35 -smp ${CORES} -vga std -serial stdio -soundhw pcspk -m ${MEM} -device isa-debug-exit,iobase=0xf4,iosize=0x04 -s -S -boot menu=on -hdb DripOS.img -hda dripdisk.img &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"
	make clean

update_qloader2:
	- rm -rf qloader2
	mkdir qloader2
	git clone https://github.com/qword-os/qloader2

# Generic rules for wildcards
# To make an object, always compile from its .c

%.o: %.c
	${CC} ${CFLAGS} -Iinclude -I src -O${O_LEVEL} -Werror -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fno-omit-frame-pointer -ffunction-sections -fdata-sections -MD -c $< -o $@ -ffreestanding

%.bin: %.real
	nasm -f bin -o $@ $<

%.o: %.s
	${CC} -Werror -Wall -Wextra -Wpedantic -Wunused-function -O${O_LEVEL} -g -MD -c $< -o $@
%.o: %.S
	${CC} -Werror -Wall -Wextra -Wpedantic -O${O_LEVEL} -g -MD -c $< -o $@

%.o: %.asm
	nasm -f elf64 -F dwarf -g -o $@ $<

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf DripOS.img
	rm -rf ${OBJ}
	rm -rf $(shell find src/ -type f -name '*.d')
