ENTRY(call_start_cpu0)

MEMORY
{
    iram : org = 0x40080000, len = 0x20000
    dram : org = 0x3FFB0000, len = 0x2C000
}

_stack_top = 0x3FFDC000;
_kernel_stack_size = 0x4000;

SECTIONS
{
    .vectors : ALIGN(0x400)
    {
        _syVectorBase = ABSOLUTE(.);
        KEEP(*(.vectors))
    } > iram
    
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

    .stacks (NOLOAD) : ALIGN(16)
    {
        *(.stacks .stacks.*)
    } > dram

    .bss : ALIGN(4)
    {
        _bss_start = ABSOLUTE(.);
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = ABSOLUTE(.);
    } > dram
}

_kernel_stack_limit = _stack_top - _kernel_stack_size;

ASSERT(_bss_end <= _kernel_stack_limit,
    "SyntropyOS: DRAM overflow: .bss reaches into the kernel stack reserve. Shrink static data or move code to XIP.")
