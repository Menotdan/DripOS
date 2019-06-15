OUTPUT_FORMAT(elf32-i386)
ENTRY(main)

SECTIONS
{
    . = 0xEFFFFF;
    .text : { *(.text) }
    .data : { *(.data) }
    .bss  : { *(.bss) *(COMMON) }
}