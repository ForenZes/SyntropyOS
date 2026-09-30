.section .vectors, "ax"
    .global _syVectorBase
_syVectorBase:

    .org 0x000
    movi a2, 0x704
    j _syWindowExc
    .org 0x040
    movi a2, 0x714
    j _syWindowExc
    .org 0x080
    movi a2, 0x708
    j _syWindowExc
    .org 0x0C0
    movi a2, 0x718
    j _syWindowExc
    .org 0x100
    movi a2, 0x70C
    j _syWindowExc
    .org 0x140
    movi a2, 0x71C
    j _syWindowExc
    .org 0x180
    j _syHalt
    .org 0x1C0
    j _syHalt
    .org 0x200
    j _syHalt
    .org 0x240
    j _syHalt
    .org 0x280
    j _syDebugHandler
    .org 0x2C0
    j _syHalt
    .org 0x300
    j _syFaultCaptureCPU
    .org 0x340
    j _syFaultCaptureCPU
    .org 0x3C0
    j _syHalt

    .section .text
    .align 4
    .global _syHalt
_syHalt:
    j _syHalt

    .align 4
    .global _syDebugHandler
_syDebugHandler:
    wsr  a2, 214
    movi a2, syFaultRegs
    s32i a0,  a2, 0
    s32i a1,  a2, 4
    rsr  a0,  214
    s32i a0,  a2, 8
    s32i a3,  a2, 12
    s32i a4,  a2, 16
    s32i a5,  a2, 20
    s32i a6,  a2, 24
    s32i a7,  a2, 28
    s32i a8,  a2, 32
    s32i a9,  a2, 36
    s32i a10, a2, 40
    s32i a11, a2, 44
    s32i a12, a2, 48
    s32i a13, a2, 52
    s32i a14, a2, 56
    s32i a15, a2, 60
    rsr  a0,  182
    s32i a0,  a2, 64
    rsr  a0,  198
    s32i a0,  a2, 68
    rsr  a0,  3
    s32i a0,  a2, 72
    rsr  a0,  233
    s32i a0,  a2, 76
    movi a0, syFaultReason
    movi a3, 1
    s32i a3, a0, 0
    movi a1, syPanicStack + 2048
    movi a0, 0
    wsr  a0, 160
    wsr  a0, 161
    dsync
    movi a0, syKernelPanicInit
    wsr  a0, 182
    rsync
    rfi  6

    .align 4
    .global _syFaultCaptureCPU
_syFaultCaptureCPU:
    wsr  a2, 209
    movi a2, syFaultRegs
    s32i a0,  a2, 0
    s32i a1,  a2, 4
    rsr  a0,  209
    s32i a0,  a2, 8
    s32i a3,  a2, 12
    s32i a4,  a2, 16
    s32i a5,  a2, 20
    s32i a6,  a2, 24
    s32i a7,  a2, 28
    s32i a8,  a2, 32
    s32i a9,  a2, 36
    s32i a10, a2, 40
    s32i a11, a2, 44
    s32i a12, a2, 48
    s32i a13, a2, 52
    s32i a14, a2, 56
    s32i a15, a2, 60
    rsr  a0,  177
    s32i a0,  a2, 64
    rsr  a0,  230
    s32i a0,  a2, 68
    rsr  a0,  3
    s32i a0,  a2, 72
    rsr  a0,  232
    s32i a0,  a2, 76
    movi a0, syFaultReason
    movi a3, 3
    s32i a3, a0, 0
    movi a1, syPanicStack + 2048
    movi a0, syKernelPanicInit
    callx0 a0
    j _syHalt

    .align 4
    .global _syWindowExc
_syWindowExc:
    movi a1, syPanicStack + 2048
    movi a0, syWindowExceptionPanic
    callx0 a0
    j _syHalt