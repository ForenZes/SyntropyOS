#include "UserSpace.h"
#include "Framebuffer.h"
#include "TouchScreen.h"
#include "TouchCalibrate.h"
#include "Kernel.h"
#include "Console.h"
#include "GraphicsCache.h"

bool SetupDoneAllSteps = false;

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

char * currentNetwork = "-";
bool isReachability = false;

static void drawIcon(int x, int y, const SyIcon *ic, uint16_t color, int scale){
    FrameBufferIcon(x, y, ic->data, ic->w, ic->h, color, scale);
}

static void switchboardSetBars(uint16_t color){
    FrameBufferFillRect(0, 0, SB_W, SB_STATUS_H, color);
    FrameBufferFillRect(0, SB_DOCK_Y, SB_W, SB_DOCK_H, color);
    FrameBufferLine(0, SB_DOCK_Y, SB_W - 1, SB_DOCK_Y, SB_ACCENT);
}

static void switchboardSetWallpaper(void){
    const char *title = "SyntropyOS";
    int tw = FrameBufferTextWidth(title, 3);
    FrameBufferText(FrameBufferCenterX(tw), 96, title, SB_INK, 3);
}

static void switchboardSetMisc(int wifiConnected){
    FrameBufferText(272, 6, "12:31", SB_INK, 1);
    drawIcon(250, 1, wifiConnected ? &wifi_signal : &no_signal, SB_INK, 1);

    const SyIcon *dock[8] = {
        &menu_icn, &cell_pad, &SMS, &settings,
        &files, &camera_app_icon, &terminal_icon, &shutdown_icon
    };
	
    for(int i = 0; i < 8; i++){
        drawIcon(i * 40 + 8, SB_ICON_Y, dock[i], SB_INK, 1);
    }
}

void buildFatalErrorAlert(void){
    FrameBufferClear(RGB(2, 126, 105));
    FrameBufferFillRect(16, 52, 289, 137, RGB(226, 64, 64));
    FrameBufferRect(15, 51, 290, 138, RGB(255, 255, 255));
    FrameBufferText(64, 74, "Fatal Error!", RGB(255, 255, 255), 2);
    FrameBufferText(32, 105, "There is no space to spawn a new", RGB(255, 255, 255), 1);
    FrameBufferText(64, 124, "thread. Reboot Syntropy?", RGB(255, 255, 255), 1);
    FrameBufferFillRect(107, 157, 106, 22, RGB(255, 117, 117));
    FrameBufferRect(106, 156, 108, 24, RGB(255, 255, 255));
    FrameBufferText(114, 160, "Reboot", RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

void buildAlertNotification(){
    FrameBufferClear(RGB(2, 126, 105));
    FrameBufferFillRect(16, 52, 289, 137, RGB(229, 143, 206));
    FrameBufferRect(15, 51, 290, 138, RGB(255, 255, 255));
    FrameBufferText(96, 68, "Welcome!", RGB(255, 255, 255), 2);
    FrameBufferText(32, 102, "This is the text for this alert!", RGB(255, 255, 255), 1);
    FrameBufferText(60, 119, "Feel free to ignore this!", RGB(255, 255, 255), 1);
    FrameBufferFillRect(107, 151, 106, 22, RGB(228, 98, 168));
    FrameBufferRect(106, 150, 108, 24, RGB(255, 255, 255));
    FrameBufferText(144, 155, "OK", RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

void switchboardDraw(const char *username, int wifiConnected){
    FrameBufferClear(SB_BG);
    switchboardSetBars(SB_BAR);
    FrameBufferText(4, 6, username, SB_INK, 1);
    switchboardSetWallpaper();
    switchboardSetMisc(wifiConnected);
    FrameBufferFlush();
    //buildAlertNotification();
    FrameBufferFlush();
}

void touchScreenCalibrationApp(){
    touchCalibrate();
    return;
}

static uint32_t networkConnStack[256];   // 1 KB each, static so they persist
static uint32_t syntropyGUIStack[256];
static syThread_t networkConnThread;
static syThread_t syntropyGUIThread;

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

void enablingWifiAlert(void){
    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferFillRect(16, 77, 289, 66, RGB(64, 118, 226));
    FrameBufferRect(15, 77, 290, 66, RGB(255, 255, 255));
    alertSetMessage("Enabling WiFi...");
    FrameBufferIcon(144, 80, cellTowerIcon, 16, 16, RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

static void *setupNetworkConn(void *arg){
    enablingWifiAlert();
    isReachability = false; // Networking TBD
    currentNetwork = NULL; // Networking TBD

    // No WiFi radio yet so imma set this to a dummy message. For now.
    syWaitMilliseconds(1000);
    alertSetMessage("No WiFi Radio!");
    syWaitMilliseconds(1000);
    return 0;
}

static void *syntropyGUI(void *arg){
    FrameBufferFlush();
    switchboardDraw((currentNetwork && currentNetwork[0]) ? currentNetwork : "No Service", isReachability);

    while(0){
        syThreadYield();
    }
    return 0;
}

void initSyntropyUserSpace(void){
    syThreadCreate(&networkConnThread, networkConnStack, sizeof(networkConnStack), setupNetworkConn, 0);
    syThreadJoin(&networkConnThread);
    syThreadCreate(&syntropyGUIThread, syntropyGUIStack, sizeof(syntropyGUIStack), syntropyGUI, 0);
    return;
}