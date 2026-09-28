    .section .text.start
    .global call_start_cpu0
    .type call_start_cpu0, @function
    .align 4
call_start_cpu0:
    movi    a1, _stack_top
    movi    a0, SyntropyKernelInit
    callx0  a0
hang:
    j       hang
 