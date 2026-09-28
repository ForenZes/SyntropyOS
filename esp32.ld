ENTRY(call_start_cpu0)

MEMORY
{
    iram : org = 0x40080000, len = 0x20000
    dram : org = 0x3FFB0000, len = 0x2C000
}

_stack_top = 0x3FFDC000;

SECTIONS
{
    .text : ALIGN(4)
    {
        *(.literal.start)
        *(.literal .literal.*)
        *(.text.start)
        *(.text .text.*)
    } > iram

    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.*)
    } > dram

    .data : ALIGN(4)
    {
        *(.data .data.*)
    } > dram

    .bss : ALIGN(4)
    {
        _bss_start = ABSOLUTE(.);
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = ABSOLUTE(.);
    } > dram
}
