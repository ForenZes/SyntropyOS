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

bool SetupDoneAllSteps = false;
bool SetupDoneClock = false;

#define SB_W 320
#define SB_H 240
#define SB_STATUS_H 18
#define SB_DOCK_H 40
#define SB_DOCK_Y (SB_H - SB_DOCK_H)
#define SB_ICON 24
#define SB_ICON_Y (SB_DOCK_Y + (SB_DOCK_H - SB_ICON) / 2)

#define SB_BG     RGB(24, 28, 52)
#define SB_BAR    RGB(58, 82, 130)
#define SB_INK    RGB(255, 255, 255)
#define SB_ACCENT RGB(96, 176, 255)
#define DOCK_START_X 8
#define DOCK_PITCH   40
#define DOCK_COUNT   8

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
    FrameBufferFillRect(272, 6, 40, 8, SB_BAR);
    FrameBufferText(272, 6, buf, SB_INK, 1);
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

static int dockHit(int tx, int ty){
    if(ty < SB_DOCK_Y || ty >= SB_H){
        return -1;
    }

    int rel = tx - DOCK_START_X;

    if(rel < 0){
        return -1;
    }
    int idx = rel / DOCK_PITCH;

    if(idx >= DOCK_COUNT){
        return -1;
    }
    return idx;
}

void alertSetMessage(const char *message);
char * currentNetwork = "-";
bool isReachability = false;

static void drawIcon(int x, int y, const SyIcon *ic, uint16_t color, int scale){
    FrameBufferIcon(x, y, ic->data, ic->w, ic->h, color, scale);
}

static void switchboardSetBars(uint16_t color){
    FrameBufferBox(0, 0, SB_W, SB_STATUS_H, color, 0, 0, 0);
    FrameBufferBox(0, SB_DOCK_Y, SB_W, SB_DOCK_H, color, 0, 0, 0);
    FrameBufferLine(0, SB_DOCK_Y, SB_W - 1, SB_DOCK_Y, SB_ACCENT);
}

static void switchboardSetWallpaper(void){
    const char *title = "SyntropyOS";
    int tw = FrameBufferTextWidth(title, 3);
    FrameBufferText(FrameBufferCenterX(tw), 96, title, SB_INK, 3);
}

static void switchboardSetMisc(int wifiConnected){
    drawDesktopClock();
    drawIcon(250, 1, wifiConnected ? &wifi_signal : &no_signal, SB_INK, 1);

    const SyIcon *dock[8] = {
        &menu_icn, &cell_pad, &SMS, &settings,
        &files, &camera_app_icon, &terminal_icon, &shutdown_icon
    };

    for(int i = 0; i < DOCK_COUNT; i++){
        drawIcon(i * DOCK_PITCH + DOCK_START_X, SB_ICON_Y, dock[i], SB_INK, 1);
    }
}

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

void switchboardDraw(const char *username, int wifiConnected){
    FrameBufferClear(SB_BG);
    switchboardSetBars(SB_BAR);
    FrameBufferText(4, 6, username, SB_INK, 1);
    switchboardSetWallpaper();
    switchboardSetMisc(wifiConnected);
    FrameBufferFlush();
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
        switchboardDraw((currentNetwork && currentNetwork[0]) ? currentNetwork : "No Service", isReachability);

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
                int idx = dockHit(x, y);
                if(idx >= 0){
                    syTouchscreenWaitRelease();
                    switch(idx){
                        case 4: diskManagerInit(); break;
                        default: break;
                    }
                    break;
                }
                syTouchscreenWaitRelease();
            }

            syThreadYield();
        }
    }
    return 0;
}