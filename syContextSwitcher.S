    .section .text
    .global syContextSwitcher
    .type syContextSwitcher, @function
    .align 4
syContextSwitcher:
    // a2 = address of the outgoing thread's savedStackPointer field
    // a3 = the incoming thread's savedStackPointer value

    addi a1, a1, -32          // grow the stack down by 32 bytes to hold saved registers (stays 16-aligned)
    s32i a0,  a1, 0           // save a0 = the address this thread should resume at later
    s32i a12, a1, 4           // a12..a15 are the only registers we must preserve on a call0 function call
    s32i a13, a1, 8
    s32i a14, a1, 12
    s32i a15, a1, 16

    s32i a1,  a2, 0           // outgoingThread->savedStackPointer = current stack pointer

    mov  a1,  a3              

    l32i a0,  a1, 0           // pull the incoming thread's resume address back into a0
    l32i a12, a1, 4           // restore its saved registers
    l32i a13, a1, 8
    l32i a14, a1, 12
    l32i a15, a1, 16
    addi a1,  a1, 32          // give back the 32-byte block, a1 is now the incoming thread's normal stack

    ret                       // jumps to a0 -> execution is now inside the incoming thread