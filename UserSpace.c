//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "UserSpace.h"
#include "Framebuffer.h"
#include "TouchScreen.h"
#include "TouchCalibrate.h"
#include "Kernel.h"
#include "Console.h"
#include "GraphicsCache.h"
#include "UART.h"
#include "DiskManager.h"
#include "SettingsApp.h"
#include "BasicApp.h"

bool SetupDoneAllSteps = false;
bool SetupDoneClock = false;

#define DT_BG   RGB(85, 119, 85)
#define DT_BAR  RGB(102, 143, 102)
#define DT_INK  RGB(163, 215, 164)
#define DT_DIM  RGB(127, 164, 127)
#define DT_LINE RGB(68, 95, 68)
#define DT_LOGO RGB(114, 151, 114)

#define OVL_BG  RGB(59, 79, 59)
#define OVL_ROW RGB(93, 131, 94)
#define OVL_X   0
#define OVL_Y   32
#define OVL_W   210
#define OVL_H   191

#define BAR_H         31
#define APPS_RIGHT    114
#define SHUTDOWN_LEFT 292

#define CLOCK_X     207
#define CLOCK_Y     8
#define CLOCK_SCALE 2

static int inRect(int x, int y, int rx, int ry, int rw, int rh){
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void formatHHMM(uint32_t secOfDay, char *out){
    int hh = (secOfDay / 3600u) % 24;
    int mm = (secOfDay % 3600u) / 60u;
    out[0] = '0' + hh / 10; out[1] = '0' + hh % 10;
    out[2] = ':';
    out[3] = '0' + mm / 10; out[4] = '0' + mm % 10;
    out[5] = 0;
}

static void drawDesktopClock(void){
    char buf[6];
    formatHHMM(syGetRealTimeSystemClock() % 86400u, buf);
    int w = FrameBufferTextWidth(buf, CLOCK_SCALE);
    FrameBufferFillRect(CLOCK_X, CLOCK_Y, w, 8 * CLOCK_SCALE, DT_BAR);
    FrameBufferText(CLOCK_X, CLOCK_Y, buf, DT_INK, CLOCK_SCALE);
}

void syClockSetupUI(void){
    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferFillRect(0, 0, 320, 31, RGB(117, 64, 125));
    FrameBufferText(40, 7, "Please set time", RGB(255, 255, 255), 2);
    FrameBufferFillRect(0, 206, 320, 34, RGB(117, 64, 125));
    FrameBufferFillRect(13, 61, 42, 118, RGB(135, 135, 135));
    FrameBufferIcon(18, 95, bigArowLeft, 16, 16, RGB(255, 255, 255), 3);
    FrameBufferFillRect(59, 61, 202, 118, RGB(203, 204, 205));
    FrameBufferFillRect(265, 61, 42, 118, RGB(135, 135, 135));
    FrameBufferIcon(255, 95, bigArowRight, 16, 16, RGB(255, 255, 255), 3);
    FrameBufferFillRect(110, 206, 100, 34, RGB(77, 40, 82));
    FrameBufferText(136, 215, "SET", RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

static void drawTimeSetValue(int hh, int mm, int field){
    char hhStr[3];
    char mmStr[3];
    hhStr[0] = '0' + hh / 10; hhStr[1] = '0' + hh % 10; hhStr[2] = 0;
    mmStr[0] = '0' + mm / 10; mmStr[1] = '0' + mm % 10; mmStr[2] = 0;

    uint16_t normal = RGB(136, 91, 143);
    uint16_t active = RGB(77, 40, 82);

    FrameBufferFillRect(59, 61, 202, 118, RGB(203, 204, 205));
    FrameBufferText(63,  100, hhStr, field == 0 ? active : normal, 5);
    FrameBufferText(143, 100, ":",   normal, 5);
    FrameBufferText(183, 100, mmStr, field == 1 ? active : normal, 5);
}

void syClockSetupRun(void){
    uint32_t nowSec = syGetRealTimeSystemClock() % 86400u;
    int hh = (int)((nowSec / 3600u) % 24u);
    int mm = (int)((nowSec % 3600u) / 60u);
    int field = 0;

    syClockSetupUI();
    drawTimeSetValue(hh, mm, field);
    FrameBufferFlush();

    for(;;){
        int x, y;
        if(touchScreenGet(&x, &y)){
            if(inRect(x, y, 13, 61, 42, 118)){
                syTouchscreenWaitRelease();
                if(field == 0){ hh = (hh + 23) % 24; } else { mm = (mm + 59) % 60; }
                drawTimeSetValue(hh, mm, field);
                FrameBufferFlush();
            } else if(inRect(x, y, 265, 61, 42, 118)){
                syTouchscreenWaitRelease();
                if(field == 0){ hh = (hh + 1) % 24; } else { mm = (mm + 1) % 60; }
                drawTimeSetValue(hh, mm, field);
                FrameBufferFlush();
            } else if(inRect(x, y, 110, 206, 100, 34)){
                syTouchscreenWaitRelease();
                if(field == 0){
                    field = 1;
                    drawTimeSetValue(hh, mm, field);
                    FrameBufferFlush();
                } else {
                    syClockSet((uint32_t)hh * 3600u + (uint32_t)mm * 60u);
                    return;
                }
            } else {
                syTouchscreenWaitRelease();
            }
        }
        syThreadYield();
    }
}

void alertSetMessage(const char *message);
char * currentNetwork = "-";
bool isReachability = false;

void buildFatalErrorAlert(void){
    FrameBufferClear(RGB(2, 126, 105));
    FrameBufferBox(15, 51, 290, 138, RGB(226, 64, 64), 10, 1, RGB(255, 255, 255));
    FrameBufferText(64, 74, "Fatal Error!", RGB(255, 255, 255), 2);
    FrameBufferText(32, 105, "There is no space to spawn a new", RGB(255, 255, 255), 1);
    FrameBufferText(64, 124, "thread. Reboot Syntropy?", RGB(255, 255, 255), 1);
    FrameBufferBox(106, 156, 108, 24, RGB(255, 117, 117), 6, 1, RGB(255, 255, 255));
    FrameBufferText(114, 160, "Reboot", RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

void enablingWifiAlert(void){
    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferBox(15, 77, 290, 66, RGB(64, 118, 226), 0, 1, RGB(255, 255, 255));
    alertSetMessage("Enabling WiFi...");
    FrameBufferIcon(144, 80, cellTowerIcon, 16, 16, RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

void syntropyDesktopDraw(void){
    FrameBufferClear(DT_BG);
    FrameBufferFillRect(0, 0, 320, 31, DT_BAR);
    FrameBufferBox(5, 3, 101, 25, DT_BG, 4, 1, DT_LINE);
    FrameBufferIcon(6, 0, appsLauncherIcon, 16, 16, DT_INK, 2);
    FrameBufferText(39, 7, "Apps", DT_INK, 2);
    FrameBufferLine(114, 0, 114, 30, DT_LINE);
    FrameBufferLine(142, 0, 142, 30, DT_LINE);
    FrameBufferLine(170, 0, 170, 30, DT_LINE);
    FrameBufferLine(198, 0, 198, 30, DT_LINE);
    FrameBufferLine(292, 0, 292, 30, DT_LINE);
    FrameBufferIcon(119, 7, noSignalIcon, 16, 16, DT_INK, 1);
    FrameBufferIcon(148, 8, soundOffIcon, 16, 16, DT_INK, 1);
    FrameBufferIcon(176, 8, batteryIcon, 16, 16, DT_INK, 1);
    drawDesktopClock();
    FrameBufferIcon(298, 7, shutdownIcon, 16, 16, DT_INK, 1);
    FrameBufferLine(0, 31, 318, 31, DT_LINE);
    FrameBufferIcon(112, 72, backgroundSyntropyIcon, 16, 16, DT_LOGO, 6);
    FrameBufferText(4, 220, "syntropyOS", DT_DIM, 2);
    FrameBufferFlush();
}

static void syntropyAppsMenuDraw(void){
    FrameBufferFillRect(OVL_X, OVL_Y, OVL_W, OVL_H, OVL_BG);
    FrameBufferFillRect(3, 35, 204, 27, OVL_ROW);
    FrameBufferFillRect(3, 64, 204, 27, OVL_ROW);
    FrameBufferFillRect(3, 93, 204, 27, OVL_ROW);
    FrameBufferFillRect(3, 122, 204, 27, OVL_ROW);
    FrameBufferFillRect(3, 151, 204, 27, OVL_ROW);
    FrameBufferRect(5, 36, 24, 24, OVL_BG);
    FrameBufferRect(5, 65, 24, 24, OVL_BG);
    FrameBufferRect(5, 94, 24, 24, OVL_BG);
    FrameBufferRect(5, 123, 24, 24, OVL_BG);
    FrameBufferRect(5, 152, 24, 24, OVL_BG);
    FrameBufferIcon(9, 40, settingsIcon, 16, 16, DT_INK, 1);
    FrameBufferText(33, 41, "Settings", DT_INK, 2);
    FrameBufferIcon(9, 69, appsHubIcon, 16, 16, DT_INK, 1);
    FrameBufferText(33, 69, "Apps Hub", DT_INK, 2);
    FrameBufferIcon(9, 97, filesIcon, 16, 16, RGB(176, 221, 177), 1);
    FrameBufferText(33, 99, "Files", DT_INK, 2);
    FrameBufferIcon(8, 127, calculatorIcon, 16, 16, DT_INK, 1);
    FrameBufferText(33, 128, "Calculator", DT_INK, 2);
    FrameBufferIcon(9, 156, calendarIcon, 16, 16, DT_INK, 1);
    FrameBufferText(33, 157, "Calendar", DT_INK, 2);
    FrameBufferFlush();
}

static int appsMenuRowHit(int x, int y){
    if(x < 3 || x >= 207){
        return -1;
    }
    int tops[5] = { 35, 64, 93, 122, 151 };
    for(int i = 0; i < 5; i++){
        if(y >= tops[i] && y < tops[i] + 27){
            return i;
        }
    }
    return -1;
}

static void syntropyAppsMenu(void){
    syntropyAppsMenuDraw();

    for(;;){
        int x, y;
        if(touchScreenGet(&x, &y)){
            syTouchscreenWaitRelease();
            if(y < BAR_H && x < APPS_RIGHT){
                return;
            }
            int row = appsMenuRowHit(x, y);
            if(row == 0){
                initSettingsApp();
                return;
            } else if(row == 1){
                syAppHubInit();
                return;
            } else if(row >= 2){
                // Files, Calculator, Calendar: no app yet
            } else if(!inRect(x, y, OVL_X, OVL_Y, OVL_W, OVL_H)){
                return;
            }
        }
        syThreadYield();
    }
}

static void syntropyPowerOff(void){
    FrameBufferClear(DT_BG);
    const char *msg = "Rebooting...";
    FrameBufferText(FrameBufferCenterX(FrameBufferTextWidth(msg, 2)), 108, msg, DT_INK, 2);
    FrameBufferFlush();
    uartPuts("syntropyOS: reboot requested\n");
    syWaitMilliseconds(1000);
    hwForceReboot();
    while(1){ }
}

void touchScreenCalibrationApp(){
    touchCalibrate();
    return;
}

void alertSetMessage(const char *message){
    int boxX = 16;
    int boxWidth = 289;
    int lineY = 119;
    int scale = 2;
    int lineHeight = 8 * scale;

    FrameBufferFillRect(boxX + 1, lineY, boxWidth - 2, lineHeight, RGB(64, 118, 226));

    int textWidth = strlen(message) * 8 * scale;
    int textX = boxX + (boxWidth - textWidth) / 2;

    FrameBufferText(textX, lineY, message, RGB(255, 255, 255), scale);
    FrameBufferFlush();
}

static void *setupNetworkConn(void *arg){
    enablingWifiAlert();
    isReachability = false; // Networking TBD
    currentNetwork = NULL; // Networking TBD

    // No WiFi radio yet so imma set this to a dummy message. For now.
    syWaitMilliseconds(1000);
    alertSetMessage("No WiFi Radio!");
    uartPuts("Reachability: No WiFi Radio!\n");
    syWaitMilliseconds(1000);
    return 0;
}

static uint32_t networkConnStack[256] __attribute__((aligned(64))); // 1 KB
static uint32_t syntropyGUIStack[1024] __attribute__((aligned(64))); // 4 KB
static syThread_t networkConnThread;
static syThread_t syntropyGUIThread;

void initSyntropyUserSpace(void){
    if(syThreadCreate(&networkConnThread, networkConnStack, sizeof(networkConnStack), setupNetworkConn, 0) != 0){
        buildFatalErrorAlert();
        while(1){

        }
    }
    syThreadJoin(&networkConnThread);

    if(syThreadCreate(&syntropyGUIThread, syntropyGUIStack, sizeof(syntropyGUIStack), syntropyDesktopMonitor, 0) != 0){
        buildFatalErrorAlert();
        while(1){

        }
    }
    return;
}

void *syntropyDesktopMonitor(void *arg){
    uartPuts("Welcome to syntropyOS Userspace!\n");
    if(!SetupDoneClock){
        syClockSetupRun();
        SetupDoneClock = true;
    }

    for(;;){
        syntropyDesktopDraw();
        int lastMinute = syGetRealTimeSystemClock() / 60u;

        for(;;){
            int minute = syGetRealTimeSystemClock() / 60u;
            if(minute != lastMinute){
                lastMinute = minute;
                drawDesktopClock();
                FrameBufferFlush();
            }

            int x, y;
            if(touchScreenGet(&x, &y)){
                if(y < BAR_H && x < APPS_RIGHT){
                    syTouchscreenWaitRelease();
                    syntropyAppsMenu();
                    break;
                } else if(y < BAR_H && x >= SHUTDOWN_LEFT){
                    syTouchscreenWaitRelease();
                    syntropyPowerOff();
                } else {
                    syTouchscreenWaitRelease();
                }
            }

            syThreadYield();
        }
    }
    return 0;
}