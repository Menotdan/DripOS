/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

OUTPUT_FORMAT(elf64-x86-64)

BSTRP_VMA_START = 0x100000;
KRNL_VMA_START = 0xFFFFFFFF80000000;

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	. = BSTRP_VMA_START;

	__bootstrap_start = .;
	/* The multiboot header needs to be early in the binary, so that GRUB 
	   can find it */
	.multiboot ALIGN(4K): AT(ADDR(.multiboot))
   	{
		*(.multiboot)
   	}
	/* Only load sections from boot.o */
   	.bootstrap ALIGN(4K): AT(ADDR(.multiboot))
   	{
    	boot.o (.text)
    	boot.o (.bss)
    	boot.o (.rodata)
    	boot.o (.data)
   	}
	__bootstrap_end = ALIGN(2M);
	/* Now put sections for the kernel at higher addresses */
	. += KRNL_VMA_START; /* Jump to the higher virtual address space */
	__kernel_start = .;

	/* All the object files except boot.o */
	.text ALIGN(4K): AT(ADDR(.text) - KRNL_VMA_START)
	{
		*(EXCLUDE_FILE(boot.o) .text)
      	*(EXCLUDE_FILE(boot.o) .rodata)
	}

	.data ALIGN(4K): AT(ADDR(.data) - KRNL_VMA_START)
	{
		*(EXCLUDE_FILE(boot.o) .data)
	}

	.bss ALIGN(4K): AT(ADDR(.bss) - KRNL_VMA_START)
	{
		*(EXCLUDE_FILE(boot.o) COMMON)
      	*(EXCLUDE_FILE(boot.o) .bss)
	}

	__kernel_end = ALIGN(2M);
}