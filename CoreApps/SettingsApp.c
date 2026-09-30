//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "SettingsApp.h"
#include "CoreStorage.h"

static const uint32_t settingsAppIcon[] = {
    0x7C00,
    0xFFFF,
    0x7C00,
    0x0000,
    0x0000,
    0x007C,
    0xFFFF,
    0x007C,
    0x0000,
    0x0000,
    0x0000,
    0x0F80,
    0xFFFF,
    0x0F80,
    0x0000,
    0x0000
};

static const uint32_t timeSetupIcon[] = {
    0x0000,
    0x0000,
    0x3FFC,
    0x2004,
    0x2044,
    0x2044,
    0x2044,
    0x2044,
    0x2044,
    0x23C4,
    0x2024,
    0x2014,
    0x2004,
    0x3FFC,
    0x0000,
    0x0000
};

static const uint32_t WiFiSetupIcon[] = {
    0x0000,
    0x0000,
    0x0000,
    0x3FFC,
    0x0000,
    0x1FF8,
    0x0000,
    0x0FF0,
    0x0000,
    0x07E0,
    0x0000,
    0x0180,
    0x0180,
    0x0000,
    0x0000,
    0x0000
};

static const uint32_t TouchCalibrationSetupIcon[] = {
    0x0000,
    0x1000,
    0x1800,
    0x0800,
    0x0D80,
    0x07C0,
    0x0660,
    0x0E30,
    0x1B18,
    0x310C,
    0x6186,
    0xC083,
    0xC001,
    0x6000,
    0x3000,
    0x0000
};

static const uint32_t DiskManagerIcon[] = {
    0x0000,
    0x0000,
    0x0FF8,
    0x0AA8,
    0x1AA8,
    0x1008,
    0x1018,
    0x1010,
    0x1010,
    0x1018,
    0x1008,
    0x1008,
    0x1008,
    0x1FF8,
    0x0000,
    0x0000
};

static const uint32_t ForceRebootIcon[] = {
    0x0000,
    0x0000,
    0x0220,
    0x0220,
    0x0220,
    0x0220,
    0x1FFC,
    0x0FF8,
    0x0FF8,
    0x0FF8,
    0x07F0,
    0x03E0,
    0x01C0,
    0x0080,
    0x0000,
    0x0000
};

static const uint32_t ExitAppIcon[] = {
    0x0000,
    0x0100,
    0x0080,
    0x0040,
    0x0020,
    0x3FF0,
    0x2020,
    0x2040,
    0x2080,
    0x2100,
    0x2000,
    0x2000,
    0x3FFC,
    0x0000,
    0x0000,
    0x0000
};

void sySettingsAppRenderUI(void){
    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferFillRect(0, 0, 320, 31, RGB(117, 64, 125));
    FrameBufferText(42, 7, "Configurations", RGB(255, 255, 255), 2);
    FrameBufferFillRect(0, 206, 320, 34, RGB(117, 64, 125));
    FrameBufferFillRect(0, 32, 320, 32, RGB(203, 204, 205));
    FrameBufferIcon(4, 1, settingsAppIcon, 16, 16, RGB(255, 255, 255), 2);
    FrameBufferText(40, 41, "Time Setup", RGB(117, 64, 125), 2);
    FrameBufferRect(1, 33, 32, 30, RGB(117, 64, 125));
    FrameBufferIcon(1, 32, timeSetupIcon, 16, 16, RGB(117, 64, 125), 2);
    FrameBufferFillRect(0, 65, 320, 32, RGB(203, 204, 205));
    FrameBufferRect(1, 66, 32, 30, RGB(117, 64, 125));
    FrameBufferText(40, 72, "WiFi Connection", RGB(117, 64, 125), 2);
    FrameBufferIcon(1, 65, WiFiSetupIcon, 16, 16, RGB(117, 64, 125), 2);
    FrameBufferFillRect(0, 98, 320, 32, RGB(203, 204, 205));
    FrameBufferRect(1, 99, 32, 30, RGB(117, 64, 125));
    FrameBufferText(40, 106, "Touch Calibration", RGB(117, 64, 125), 2);
    FrameBufferIcon(1, 98, TouchCalibrationSetupIcon, 16, 16, RGB(117, 64, 125), 2);
    FrameBufferFillRect(0, 131, 320, 32, RGB(203, 204, 205));
    FrameBufferRect(1, 132, 32, 30, RGB(117, 64, 125));
    FrameBufferText(40, 138, "Disk Manager", RGB(117, 64, 125), 2);
    FrameBufferIcon(1, 131, DiskManagerIcon, 16, 16, RGB(117, 64, 125), 2);
    FrameBufferFillRect(0, 164, 320, 32, RGB(210, 211, 212));
    FrameBufferRect(1, 165, 32, 30, RGB(117, 64, 125));
    FrameBufferText(40, 171, "Force Reboot", RGB(117, 64, 125), 2);
    FrameBufferIcon(1, 164, ForceRebootIcon, 16, 16, RGB(117, 64, 125), 2);
    FrameBufferIcon(1, 208, ExitAppIcon, 16, 16, RGB(255, 255, 255), 2);
    FrameBufferFlush();
}

static int hitRow(int x, int y, int rowY){
    return x >= 0 && x < 320 && y >= rowY && y < rowY + 32;
}

static void noWifiToast(void){
    FrameBufferFillRect(30, 96, 260, 44, RGB(48, 48, 52));
    FrameBufferRect(30, 96, 260, 44, RGB(255, 255, 255));
    int tw = FrameBufferTextWidth("No WiFi Radio!", 2);
    FrameBufferText(160 - tw / 2, 110, "No WiFi Radio!", RGB(255, 194, 194), 2);
    FrameBufferFlush();
    syWaitMilliseconds(1100);
}

void initSettingsApp(void){
    for(;;){
        sySettingsAppRenderUI();

        int done = 0;
        while(!done){
            int x, y;
            if(touchScreenGet(&x, &y)){
                if(hitRow(x, y, 32)){
                    syTouchscreenWaitRelease();
                    syClockSetupRun();
                    done = 1;
                } else if(hitRow(x, y, 65)){
                    syTouchscreenWaitRelease();
                    noWifiToast();
                    done = 1;
                } else if(hitRow(x, y, 98)){
                    syTouchscreenWaitRelease();
                    touchScreenCalibrationApp();
                    done = 1;
                } else if(hitRow(x, y, 131)){
                    syTouchscreenWaitRelease();
                    diskManagerInit();
                    done = 1;
                } else if(hitRow(x, y, 164)){
                    syTouchscreenWaitRelease();
                    hwForceReboot();
                } else if(x >= 0 && x < 320 && y >= 206 && y < 240){
                    syTouchscreenWaitRelease();
                    return;
                } else {
                    syTouchscreenWaitRelease();
                }
            }
            syThreadYield();
        }
    }
}