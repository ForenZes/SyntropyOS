//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include "TouchScreen.h"
#include "Framebuffer.h"
#include "UserSpace.h"
#include "Kernel.h"

#define CAL_INSET 24
#define CAL_BG    RGB(12, 16, 34)
#define CAL_INK   RGB(230, 238, 255)
#define CAL_MARK  RGB(96, 200, 120)
#define CAL_DIM   RGB(120, 140, 180)

static void coarseDelay(int n){
    for(volatile int i = 0; i < n; i++){
        __asm__ volatile("nop");
    }
}

static void drawPlus(int cx, int cy, uint16_t col){
    FrameBufferFillRect(cx - 10, cy - 1, 21, 3, col);
    FrameBufferFillRect(cx - 1, cy - 10, 3, 21, col);
}

static void waitRelease(void){
    uint16_t rx, ry;
    int quiet = 0;
    while(quiet < 3){
        if(touchScreenGetRaw(&rx, &ry)){
            quiet = 0;
        }
        else{
            quiet++;
        }
        coarseDelay(30000);
    }
}

static void waitPress(uint16_t *rx, uint16_t *ry){
    while(!touchScreenGetRaw(rx, ry)){
        coarseDelay(5000);
    }
}

static void uStr(int v, char *b){
    char t[12];
    int n = 0;
    int neg = 0;
    unsigned u;
    if(v < 0){
        neg = 1;
        u = (unsigned)(-v);
    }
    else{
        u = (unsigned)v;
    }
    if(u == 0){
        t[n++] = '0';
    }
    while(u){
        t[n++] = '0' + (u % 10);
        u /= 10;
    }
    int j = 0;
    if(neg){
        b[j++] = '-';
    }
    while(n){
        b[j++] = t[--n];
    }
    b[j] = 0;
}

static void drawKV(const char *k, int v, int x, int y){
    char num[12];
    uStr(v, num);
    FrameBufferText(x, y, k, CAL_DIM, 1);
    FrameBufferText(x + 120, y, num, CAL_INK, 1);
}

void touchCalibrate(void){
    int tx[4] = { 
        CAL_INSET, SCREEN_W - 1 - CAL_INSET, 
        CAL_INSET, SCREEN_W - 1 - CAL_INSET 
    };
    
    int ty[4] = { CAL_INSET, CAL_INSET, SCREEN_H - 1 - CAL_INSET, SCREEN_H - 1 - CAL_INSET };
    uint16_t rx[4], ry[4];

    for(int i = 0; i < 4; i++){
        FrameBufferClear(CAL_BG);
        const char *msg = "Touch the marker";
        FrameBufferText(FrameBufferCenterX(FrameBufferTextWidth(msg, 1)), SCREEN_H / 2 - 6, msg, CAL_INK, 1);
        drawPlus(tx[i], ty[i], CAL_MARK);
        FrameBufferFlush();
        syTouchscreenWaitRelease();
        waitPress(&rx[i], &ry[i]);
    }

    int dxr = rx[1] - rx[0];
    if(dxr < 0){
        dxr = -dxr;
    }
    int dyr = ry[1] - ry[0];
    if(dyr < 0){
        dyr = -dyr;
    }
    int swap = (dyr > dxr) ? 1 : 0;

    int axFor[4], ayFor[4];
    for(int i = 0; i < 4; i++){
        axFor[i] = swap ? ry[i] : rx[i];
        ayFor[i] = swap ? rx[i] : ry[i];
    }

    int leftX  = (axFor[0] + axFor[2]) / 2;
    int rightX = (axFor[1] + axFor[3]) / 2;
    int topY   = (ayFor[0] + ayFor[1]) / 2;
    int botY   = (ayFor[2] + ayFor[3]) / 2;

    int spanX = (SCREEN_W - 1 - CAL_INSET) - CAL_INSET;
    int spanY = (SCREEN_H - 1 - CAL_INSET) - CAL_INSET;

    int deltaX = rightX - leftX;
    int deltaY = botY - topY;

    int edgeX = spanX != 0 ? deltaX * CAL_INSET / spanX : 0;
    int edgeY = spanY != 0 ? deltaY * CAL_INSET / spanY : 0;

    int raw0X = leftX  - edgeX;
    int rawEX = rightX + edgeX;
    int raw0Y = topY   - edgeY;
    int rawEY = botY   + edgeY;

    if(raw0X <= rawEX){
        TOUCH_MIN_X = raw0X;
        TOUCH_MAX_X = rawEX;
        TOUCH_FLIP_X = 0;
    }
    else{
        TOUCH_MIN_X = rawEX;
        TOUCH_MAX_X = raw0X;
        TOUCH_FLIP_X = 1;
    }
    if(raw0Y <= rawEY){
        TOUCH_MIN_Y = raw0Y;
        TOUCH_MAX_Y = rawEY;
        TOUCH_FLIP_Y = 0;
    }
    else{
        TOUCH_MIN_Y = rawEY;
        TOUCH_MAX_Y = raw0Y;
        TOUCH_FLIP_Y = 1;
    }
    TOUCH_SWAP_XY = swap;

    syTouchscreenWaitRelease();
    FrameBufferClear(CAL_BG);
    const char *title = "Calibration saved";
    FrameBufferText(FrameBufferCenterX(FrameBufferTextWidth(title, 2)), 20, title, CAL_MARK, 2);

    int y = 70;
    drawKV("TOUCH_MIN_X", TOUCH_MIN_X, 40, y); y += 18;
    drawKV("TOUCH_MAX_X", TOUCH_MAX_X, 40, y); y += 18;
    drawKV("TOUCH_MIN_Y", TOUCH_MIN_Y, 40, y); y += 18;
    drawKV("TOUCH_MAX_Y", TOUCH_MAX_Y, 40, y); y += 18;
    drawKV("TOUCH_SWAP_XY", TOUCH_SWAP_XY, 40, y); y += 18;
    drawKV("TOUCH_FLIP_X", TOUCH_FLIP_X, 40, y); y += 18;
    drawKV("TOUCH_FLIP_Y", TOUCH_FLIP_Y, 40, y); y += 22;
    const char *tap = "Tap to continue";
    FrameBufferText(FrameBufferCenterX(FrameBufferTextWidth(tap, 1)), y, tap, CAL_DIM, 1);
    FrameBufferFlush();

    uint16_t dx, dy;
    waitPress(&dx, &dy);
    syTouchscreenWaitRelease();

    SetupDoneAllSteps = true;
    return;
}
