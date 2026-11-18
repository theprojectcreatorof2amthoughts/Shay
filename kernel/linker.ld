OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x100000;
    .limine_requests_start : ALIGN(8) { KEEP(*(.limine_requests_start)) } :text
    .limine_requests : ALIGN(8) { KEEP(*(.limine_requests)) } :text
    .limine_requests_end : ALIGN(8) { KEEP(*(.limine_requests_end)) } :text
    .text : ALIGN(0x1000) { *(.text .text.*) } :text
    .rodata : ALIGN(0x1000) { *(.rodata .rodata.*) } :text
    .data : ALIGN(0x1000) { *(.data .data.*) } :data
    .bss : ALIGN(0x1000) { *(COMMON) *(.bss .bss.*) } :data

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
