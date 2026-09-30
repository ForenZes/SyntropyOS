//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include <stdint.h>
#include "Kernel.h"
#include "Framebuffer.h"
#include "Console.h"
#include "UserSpace.h"
#include <stddef.h>
#include "TouchScreen.h"
#include "UART.h"
#include "DeviceTree.h"
#include "CoreStorage.h"
#include "DiskManager.h"

#define kSyntropyKernelVersion "SyntropyOS Kernel v0.01 ALPHA ~ DEVELOPMENT / GeoSn0w, Sep 28, 2026"

typedef int kMutex_t; // kern level mutexes
typedef int uMutex_t; // userland mutexes
typedef int kSemaphore_t; // Kernel semaphore cookie

#define ESP32_RTC_CNTL_BASE     0x3FF48000u
#define ESP32_RESET_REG         (ESP32_RTC_CNTL_BASE + 0x00u)
#define ESP32_RESET_BIT         31

#define ESP32S2_RTC_CNTL_BASE   0x3F408000u
#define ESP32S2_RESET_REG       (ESP32S2_RTC_CNTL_BASE + 0x00u)
#define ESP32S2_RESET_BIT       31

#define ESP32S3_RTC_CNTL_BASE   0x60008000u
#define ESP32S3_RESET_REG       (ESP32S3_RTC_CNTL_BASE + 0x00u)
#define ESP32S3_RESET_BIT       31

#define ESP32C3_RTC_CNTL_BASE   0x60008000u
#define ESP32C3_RESET_REG       (ESP32C3_RTC_CNTL_BASE + 0x00u)
#define ESP32C3_RESET_BIT       31


#define DR_REG_GPIO        0x3FF44000
#define DR_REG_IOMUX       0x3FF49000
#define DR_REG_SPI2        0x3FF64000
#define DR_REG_DPORT       0x3FF00000
#define CPU_HZ             40000000
#define MMIO32(addr)       (*(volatile uint32_t *)(addr))

#define GPIO_OUT_W1TS      (*(volatile uint32_t*)(DR_REG_GPIO + 0x08))
#define GPIO_OUT_W1TC      (*(volatile uint32_t*)(DR_REG_GPIO + 0x0C))
#define GPIO_ENABLE_W1TS   (*(volatile uint32_t*)(DR_REG_GPIO + 0x24))
#define GPIO_FUNC_OUT_SEL(n) (*(volatile uint32_t*)(DR_REG_GPIO + 0x530 + (n)*4))

#define IOMUX_GPIO2        (*(volatile uint32_t*)(DR_REG_IOMUX + 0x40))
#define IOMUX_GPIO13       (*(volatile uint32_t*)(DR_REG_IOMUX + 0x38))
#define IOMUX_GPIO14       (*(volatile uint32_t*)(DR_REG_IOMUX + 0x30))
#define IOMUX_GPIO15       (*(volatile uint32_t*)(DR_REG_IOMUX + 0x3C))
#define IOMUX_GPIO21       (*(volatile uint32_t*)(DR_REG_IOMUX + 0x7C))

#define DPORT_PERIP_CLK_EN (*(volatile uint32_t*)(DR_REG_DPORT + 0xC0))
#define DPORT_PERIP_RST_EN (*(volatile uint32_t*)(DR_REG_DPORT + 0xC4))

#define SPI_CMD_REG        (*(volatile uint32_t*)(DR_REG_SPI2 + 0x00))
#define SPI_CTRL_REG       (*(volatile uint32_t*)(DR_REG_SPI2 + 0x08))
#define SPI_CLOCK_REG      (*(volatile uint32_t*)(DR_REG_SPI2 + 0x18))
#define SPI_USER_REG       (*(volatile uint32_t*)(DR_REG_SPI2 + 0x1C))
#define SPI_USER1_REG      (*(volatile uint32_t*)(DR_REG_SPI2 + 0x20))
#define SPI_MOSI_DLEN_REG  (*(volatile uint32_t*)(DR_REG_SPI2 + 0x28))
#define SPI_PIN_REG        (*(volatile uint32_t*)(DR_REG_SPI2 + 0x34))
#define SPI_W0             ((volatile uint32_t*)(DR_REG_SPI2 + 0x80))
#define SPI_USER2_REG      (*(volatile uint32_t*)(DR_REG_SPI2 + 0x24))

#define RTC_CNTL_WDTCONFIG0  (*(volatile uint32_t*)0x3FF4808C)
#define RTC_CNTL_WDTWPROTECT (*(volatile uint32_t*)0x3FF480A4)
#define TIMG0_WDTCONFIG0     (*(volatile uint32_t*)0x3FF5F048)
#define TIMG0_WDTWPROTECT    (*(volatile uint32_t*)0x3FF5F064)
#define WDT_WKEY             0x50D83AA1

#define PIN_CS   15
#define PIN_DC   2
#define PIN_BL   21

#define LCD_MADCTL 0x60

#define SY_STACK_FILL   0xC5C5C5C5
#define SY_STACK_CANARY 0x5A4E7D09
uint32_t syPanicStack[512] __attribute__((section(".stacks"), aligned(16)));
void syKernelPanicInit(void);

uint32_t syFaultRegs[20];

void hwForceReboot(void){

    if (deviceType == 1) {
        MMIO32(ESP32_RESET_REG) |= (1u << ESP32_RESET_BIT); // ESP32 Classic
    } else if (deviceType == 2) {
        MMIO32(ESP32S2_RESET_REG) |= (1u << ESP32S2_RESET_BIT); // ESP32 S2
    } else if (deviceType == 3) {
        MMIO32(ESP32S3_RESET_REG) |= (1u << ESP32S3_RESET_BIT); // ESP32 S3
    } else if (deviceType == 4) {
        MMIO32(ESP32C3_RESET_REG) |= (1u << ESP32C3_RESET_BIT); // ESP32 C3
    }

    while (0) {
        // bail
    }
}

// Threading subsystem
static syThread_t kernelThread;
static syThread_t *currentRunningThread = 0;
static int nextThreadId = 1;

syThread_t *syCurrentThread(void){
    return currentRunningThread;
}

syThreadReturn_t syThreadCreate(syThread_t *newThread, void *stackMemory, uint32_t stackSizeBytes, void *(*threadEntry)(void *), void *threadArgument){
    if(newThread == 0 || stackMemory == 0 || stackSizeBytes < 256 || currentRunningThread == 0){
        return -1;
    }

    newThread->threadEntry = threadEntry;
    newThread->threadArgument = threadArgument;
    newThread->threadResult = 0;
    newThread->stackMemory = (uint32_t *)stackMemory;
    newThread->stackSizeBytes = stackSizeBytes;
    newThread->threadState = SY_READY;
    newThread->threadId = nextThreadId++;


    syStackPaint(newThread);
    newThread->stackMemory[0] = SY_STACK_CANARY;

    uintptr_t stackTopAligned = ((uintptr_t)stackMemory + stackSizeBytes) & ~(uintptr_t)0xF;
    uint32_t *fakeSavedRegisters = (uint32_t *)(stackTopAligned - 32);

    fakeSavedRegisters[0] = (uint32_t)(uintptr_t)syThreadTrampoline;
    fakeSavedRegisters[1] = 0;
    fakeSavedRegisters[2] = 0;
    fakeSavedRegisters[3] = 0;
    fakeSavedRegisters[4] = 0;


    newThread->savedStackPointer = fakeSavedRegisters;
    newThread->nextInReadyRing = currentRunningThread->nextInReadyRing;
    currentRunningThread->nextInReadyRing = newThread;

    return 0;
}

void syThreadYield(void){
    syThread_t *outgoingThread = currentRunningThread;
    syThread_t *nextThread = outgoingThread->nextInReadyRing;

    while(nextThread != outgoingThread && nextThread->threadState != SY_READY){
        nextThread = nextThread->nextInReadyRing; // walk the ring, find next vict... thread!
    }

    if(nextThread == outgoingThread){
        return; // no thread qualified to run? ok...
    }

    if(outgoingThread->threadState == SY_RUNNING){
        outgoingThread->threadState = SY_READY; // A finished thread should stay finished.
    }

    nextThread->threadState = SY_RUNNING;
    currentRunningThread = nextThread;

    if(nextThread->stackMemory){
        syArmStackWatchpoint(nextThread->stackMemory);
    } else {
        syDisableStackWatchpoint();
    }

    syContextSwitcher(&outgoingThread->savedStackPointer, nextThread->savedStackPointer);
}

static void syThreadTrampoline(void){
    syThread_t *thisThread = currentRunningThread;
    thisThread -> threadResult = thisThread -> threadEntry(thisThread -> threadArgument);
    thisThread -> threadState = SY_DONE;
    
    while (1){
        syThreadYield();
    }
}

void syThreadJoin(syThread_t *thread){
    while(thread->threadState != SY_DONE){
        syThreadYield();
    }
}

void syThreadingInit(syThread_t *kernelTask){
    // Kern becomes thread 0 so the very first switch has a valid place to save its stack pointer
    kernelTask->threadId = 0;
    kernelTask->threadEntry = 0;
    kernelTask->threadArgument = 0;
    kernelTask->threadResult = 0;
    kernelTask->stackMemory = 0;
    kernelTask->stackSizeBytes = 0;
    kernelTask->threadState = SY_RUNNING;
    kernelTask->nextInReadyRing = kernelTask;   // points to itself for now.

    currentRunningThread = kernelTask;
}

static void syStackPaint(syThread_t *thread){
    uint32_t *stackWord = thread->stackMemory;
    uint32_t *stackEnd = (uint32_t *)((uintptr_t)thread->stackMemory + thread->stackSizeBytes);
    while(stackWord < stackEnd){
        *stackWord++ = SY_STACK_FILL;
    }
}

uint32_t syStackHighWater(syThread_t *thread){
    uint32_t *stackWord = thread->stackMemory;
    uint32_t *stackEnd = (uint32_t *)((uintptr_t)thread->stackMemory + thread->stackSizeBytes);
    while(stackWord < stackEnd && *stackWord == SY_STACK_FILL){
        stackWord++;
    }
    return (uint32_t)((uintptr_t)stackEnd - (uintptr_t)stackWord);
}

static void syArmStackWatchpoint(uint32_t *stackBase){
    uint32_t control = 0x80000000;
    __asm__ volatile("wsr %0, 144" :: "r"(stackBase));
    __asm__ volatile("wsr %0, 160" :: "r"(control));
    __asm__ volatile("dsync");
}

static void syDisableStackWatchpoint(void){
    uint32_t off = 0;
    __asm__ volatile("wsr %0, 160" :: "r"(off));
    __asm__ volatile("dsync");
}

static void syVectorInit(void){
    extern uint32_t _syVectorBase;
    uint32_t base = (uint32_t)(uintptr_t)&_syVectorBase;
    __asm__ volatile("wsr %0, 231; isync" :: "r"(base));

    uint32_t check;
    __asm__ volatile("rsr %0, 231" : "=r"(check));

    uartPuts("VECBASE want=");
    uartPrintHex(base);
    uartPuts(" got=");
    uartPrintHex(check);
    uartPuts("\n");

    uint32_t scratch;
    __asm__ volatile("rsil %0, 0" : "=r"(scratch));
}

static void syDecToStr(int value, char *out){
    char reversedDigits[12];
    int digitCount = 0;
    unsigned magnitude = (unsigned)value;

    if(magnitude == 0){
        reversedDigits[digitCount++] = '0';
    }
    while(magnitude){
        reversedDigits[digitCount++] = '0' + (magnitude % 10);
        magnitude /= 10;
    }

    int outIndex = 0;
    while(digitCount){
        out[outIndex++] = reversedDigits[--digitCount];
    }
    out[outIndex] = 0;
}

static void syHexToStr(uint32_t value, char *out){
    static const char hexDigits[] = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for(int i = 0; i < 8; i++){
        out[2 + i] = hexDigits[(value >> ((7 - i) * 4)) & 0xF];
    }
    out[10] = 0;
}

void syKernelPanicInit(void){
    char buf[12];
    uint32_t pc = syFaultRegs[16];
    uint32_t sp = syFaultRegs[1];
    int tid = currentRunningThread ? currentRunningThread->threadId : -1;

    uartPuts("\n!!! syntropyOS Kernel PANIC: STACK OVERFLOW !!!\n");
    uartPuts("Thread: ");
    uartPrintDec(tid);
    uartPuts("\n");

    for(int i = 0; i < 16; i++){
        uartPutc('a');
        uartPrintDec(i);
        uartPuts(i < 10 ? "  = " : " = ");
        uartPrintHex(syFaultRegs[i]);
        uartPuts("\n");
    }

    uartPuts("PC  = "); uartPrintHex(pc);              uartPuts("\n");
    uartPuts("PS  = "); uartPrintHex(syFaultRegs[17]); uartPuts("\n");
    uartPuts("SAR = "); uartPrintHex(syFaultRegs[18]); uartPuts("\n");
    uartPuts("DBG = "); uartPrintHex(syFaultRegs[19]); uartPuts("\n");
    uartPuts("\nPlease report this panic on https://forenzes.com/syntropyOS/support\n");
    uartPuts("-- The device will self reboot in 2 minutes. --\n");

    syConsoleInit(COLOR_BLACK, COLOR_WHITE, 1);
    syConsolePutString("!!! syntropyOS Kernel PANIC !!!\n");
    syConsolePutString("Stack Overflow\n\n");

    syConsolePutString("Thread: ");
    syDecToStr(tid, buf);
    syConsolePutString(buf);
    syConsolePutString("\nPC  ");
    syHexToStr(pc, buf);
    syConsolePutString(buf);
    syConsolePutString("\nA0  ");
    syHexToStr(syFaultRegs[0], buf);
    syConsolePutString(buf);
    syConsolePutString("\nSP  ");
    syHexToStr(sp, buf);
    syConsolePutString(buf);
    syConsolePutString("\n\nStackshot:\n");

    uint32_t *stackWords = (uint32_t *)(uintptr_t)sp;
    for(int i = 0; i < 8; i++){
        syHexToStr((uint32_t)(uintptr_t)(stackWords + i), buf);
        syConsolePutString(buf);
        syConsolePutString(": ");
        syHexToStr(stackWords[i], buf);
        syConsolePutString(buf);
        syConsolePutChar('\n');
    }

    syConsolePutString("The device will reboot in 2 minutes.\n");
    syWaitMilliseconds(120000);
    hwForceReboot();
}

void syFaultPanic(uint32_t exceptionCause, uint32_t faultingProgramCounter){
    uartPuts("CPU FAULT cause=");
    uartPrintHex(exceptionCause);
    uartPuts(" pc=");
    uartPrintHex(faultingProgramCounter);
    uartPuts("\n");
    while(1){
    }
}
// End of threading sub system

void *memcpy(void *dst, const void *src, size_t n){
    uint8_t *d = dst;
    const uint8_t *s = src;
    while(n--){
        *d++ = *s++;
    }
    return dst;
}
 
void *memmove(void *dst, const void *src, size_t n){
    uint8_t *d = dst;
    const uint8_t *s = src;
    if(d == s || n == 0){
        return dst;
    }
    if(d < s){
        while(n--){
            *d++ = *s++;
        }
    } else{
        d += n;
        s += n;
        while(n--){
            *--d = *--s;
        }
    }
    return dst;
}
 
void *memset(void *dst, int v, size_t n){
    uint8_t *d = dst;
    while(n--){
        *d++ = (uint8_t)v;
    }
    return dst;
}
 
int memcmp(const void *a, const void *b, size_t n){
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    while(n--){
        if(*pa != *pb){
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
    }
    return 0;
}
 
size_t strlen(const char *s){
    const char *p = s;
    while(*p){
        p++;
    }
    return (size_t)(p - s);
}

static uint32_t ccount(void){
    uint32_t c;
    __asm__ volatile("rsr %0, ccount" : "=r"(c));
    return c;
}

void syWaitMilliseconds(uint32_t ms){
    while(ms > 0){
        uint32_t chunk = (ms > 1000) ? 1000 : ms;
        uint32_t start = ccount();
        uint32_t wait = chunk * (CPU_HZ / 1000);
        while((ccount() - start) < wait){
        }
        ms -= chunk;
    }
}

static void ioSetHigh(int p){
    GPIO_OUT_W1TS = (1u << p);
}

static void ioSetLow(int p){
    GPIO_OUT_W1TC = (1u << p);
}

static void delay(volatile uint32_t n){
    while(n--){
        __asm__ volatile("nop");
    }
}

static void gpioMakeOutput(int p){
    GPIO_ENABLE_W1TS = (1u << p);
    GPIO_FUNC_OUT_SEL(p) = 0x100;
}

static void disableWatchdogs(void){
    RTC_CNTL_WDTWPROTECT = WDT_WKEY;
    RTC_CNTL_WDTCONFIG0 = 0;
    RTC_CNTL_WDTWPROTECT = 0;
    TIMG0_WDTWPROTECT = WDT_WKEY;
    TIMG0_WDTCONFIG0 = 0;
    TIMG0_WDTWPROTECT = 0;
}

static void initSPIInterfaces(void){
    DPORT_PERIP_CLK_EN |= (1u << 6);
    DPORT_PERIP_RST_EN &= ~(1u << 6);

    IOMUX_GPIO14 = (2u << 12);
    IOMUX_GPIO13 = (2u << 12);
    GPIO_FUNC_OUT_SEL(14) = 8;
    GPIO_FUNC_OUT_SEL(13) = 10;
    GPIO_ENABLE_W1TS = (1u << 14) | (1u << 13);

    SPI_CLOCK_REG = 0x00001001;
    SPI_CTRL_REG = 0;
    SPI_USER_REG = (1u << 27);
    SPI_USER1_REG = 0;
    SPI_PIN_REG = 0;
    SPI_USER2_REG = 0;
}

static void spiTx8(uint8_t b){
    SPI_W0[0] = b;
    SPI_MOSI_DLEN_REG = 7;
    SPI_CMD_REG = (1u << 18);
    while(SPI_CMD_REG & (1u << 18)){
    }
}

static void writeLCDCMD(uint8_t c){
    ioSetLow(PIN_DC);
    ioSetLow(PIN_CS);
    spiTx8(c);
    ioSetHigh(PIN_CS);
}

static void writeLCDData(uint8_t d){
    ioSetHigh(PIN_DC);
    ioSetLow(PIN_CS);
    spiTx8(d);
    ioSetHigh(PIN_CS);
}

void lcdWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
    writeLCDCMD(0x2A);
    writeLCDData(x0 >> 8);
    writeLCDData(x0 & 0xFF);
    writeLCDData(x1 >> 8);
    writeLCDData(x1 & 0xFF);
    writeLCDCMD(0x2B);
    writeLCDData(y0 >> 8);
    writeLCDData(y0 & 0xFF);
    writeLCDData(y1 >> 8);
    writeLCDData(y1 & 0xFF);
    writeLCDCMD(0x2C);
}

void lcdWritePixels(const uint16_t *pixels, uint32_t count){
    ioSetHigh(PIN_DC);
    ioSetLow(PIN_CS);
    uint32_t i = 0;

    while(i < count){
        uint32_t chunk = count - i;
        if(chunk > 32){
            chunk = 32;
        }
        uint32_t words = (chunk + 1) / 2;
        for(uint32_t w = 0; w < words; w++){
            uint32_t idx = i + w * 2;
            uint16_t a = pixels[idx];
            uint32_t v = (uint32_t)(a >> 8) | ((uint32_t)(a & 0xFF) << 8);
            if(idx + 1 < i + chunk){
                uint16_t b = pixels[idx + 1];
                v |= (uint32_t)(b >> 8) << 16;
                v |= (uint32_t)(b & 0xFF) << 24;
            }
            SPI_W0[w] = v;
        }
        SPI_MOSI_DLEN_REG = (chunk * 16) - 1;
        SPI_CMD_REG = (1u << 18);
        while(SPI_CMD_REG & (1u << 18)){
        }
        i += chunk;
    }

    ioSetHigh(PIN_CS);
}

static void initializeLiquidCristalDisplay(void){
    writeLCDCMD(0x01);
    delay(2000000);
    writeLCDCMD(0x11);
    delay(2000000);
    writeLCDCMD(0x3A);
    writeLCDData(0x55);
    writeLCDCMD(0x36);
    writeLCDData(LCD_MADCTL);
    writeLCDCMD(0x20);
    writeLCDCMD(0x13);
    writeLCDCMD(0x29);
    delay(500000);
}

static void hardwareInit(void){
    IOMUX_GPIO2 = (2u << 12);
    IOMUX_GPIO15 = (2u << 12);
    IOMUX_GPIO21 = (2u << 12);
    gpioMakeOutput(PIN_CS);
    gpioMakeOutput(PIN_DC);
    gpioMakeOutput(PIN_BL);

    ioSetHigh(PIN_BL);
    ioSetHigh(PIN_CS);

    initSPIInterfaces();
    initializeLiquidCristalDisplay();
    initTouchScreen();
}

int initCoreStorageDevices(){
    if(coreStorageInit()){
        uartPuts("CoreStorage: SD Initialization completed. SD Card Type = ");
        uartPrintDec(sdCardGetType());
        uartPuts("\n");
        uint8_t buf[512];

        if(sdReadBlock(0, buf)){
            uartPuts("CoreStorage: Card Signature: ");
            uartPrintHex(buf[510]);
            uartPrintHex(buf[511]);
            uartPuts("\n");
        } else {
            uartPuts("CoreStorage: SD card initialization failed!\n");
            return -1;
        }

        if(fatMount()){
            uartPuts("CoreStorage: FAT Partition mounted!\n");
            return 0;
        } else {
            uartPuts("CoreStorage: FAT Partition mount failed!\n");
            return -3;
        }
    }
    return -2;
}

void SyntropyKernelInit(void){
    disableWatchdogs();

    extern uint32_t _bss_start, _bss_end;
    for(uint32_t *p = &_bss_start; p < &_bss_end; p++){
        *p = 0;
    }

    hardwareInit();
    uartInit();

    syThreadingInit(&kernelThread);
    syVectorInit();

    if (!SetupDoneAllSteps) {
        touchScreenCalibrationApp();
    }

    initSyntropyUserSpace();

    while(1){
        syThreadYield();
    }
}
