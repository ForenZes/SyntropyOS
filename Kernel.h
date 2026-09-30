//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

// Memory utils
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int v, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

// Display tools
void lcdWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcdWritePixels(const uint16_t *pixels, uint32_t count);

// Threading utils
extern void syContextSwitcher(uint32_t **savedStackPointerSlot, uint32_t *nextStackPointer);
void SyntropyKernelInit(void);

typedef enum { 
    false, true 
} bool;

typedef enum { 
    SY_READY, 
    SY_RUNNING, 
    SY_BLOCKED, 
    SY_DONE 
} syThreadState_t;

typedef struct syThread_t {
    int threadId;
    void *(*threadEntry)(void *);
    void *threadArgument;
    void *threadResult;
    uint32_t *savedStackPointer;               
    uint32_t *stackMemory;                     
    uint32_t stackSizeBytes;
    syThreadState_t threadState;
    struct syThread_t *nextInReadyRing;
} syThread_t;

typedef int syThreadReturn_t;
syThreadReturn_t syThreadCreate(syThread_t *newThread, void *stackMemory, uint32_t stackSizeBytes, void *(*threadEntry)(void *), void *threadArgument);
syThread_t *syCurrentThread(void);
void syThreadingInit(syThread_t *kernelTask);
void syThreadYield(void);
void syThreadJoin(syThread_t *thread);
static void syThreadTrampoline(void);
void syWaitMilliseconds(uint32_t ms);
static void syStackPaint(syThread_t *thread);
uint32_t syStackHighWater(syThread_t *thread);
void syWatchpointPanic(uint32_t faultingProgramCounter);
void syFaultPanic(uint32_t exceptionCause, uint32_t faultingProgramCounter);
static void syArmStackWatchpoint(uint32_t *canaryAddress);
static void syDisableStackWatchpoint(void);
static void syVectorInit(void);
static void syDecToStr(int value, char *out);
static void syHexToStr(uint32_t value, char *out);
int initCoreStorageDevices(void);
#endif
