/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

OUTPUT_FORMAT(elf64-x86-64)

KRNL_VMA_START = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KRNL_VMA_START + 0x100000;

    __kernel_start = ALIGN(4K);

    .multiboot ALIGN(4K):  AT(ADDR(.multiboot) - KRNL_VMA_START)
       {*(.multiboot)}

    .text ALIGN(4K): AT(ADDR(.text) - KRNL_VMA_START)
    {*(.text*)}
    .rodata ALIGN(4K): AT(ADDR(.rodata) - KRNL_VMA_START)
    {*(.rodata*)}
    .data ALIGN(4K): AT(ADDR(.data) - KRNL_VMA_START)
    {*(.data*)}
    .bss ALIGN(4K): AT(ADDR(.bss) - KRNL_VMA_START)
    {*(.bss*) *(COMMON)}

    __kernel_end = ALIGN(4K);
}