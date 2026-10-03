//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "BasicInternal.h"
#include "Framebuffer.h"
#include "TouchScreen.h"
#include "UART.h"

static int lastTouchX;
static int lastTouchY;

void syBasicGFXClear(int color){
    FrameBufferClear((uint16_t)color);
}

void syBasicGFXFill(int x, int y, int width, int height, int color){
    FrameBufferFillRect(x, y, width, height, (uint16_t)color);
}

void syBasicGFXBox(int x, int y, int width, int height, int color){
    FrameBufferRect(x, y, width, height, (uint16_t)color);
}

void syBasicGFXText(int x, int y, const char *text, int color, int scale){
    if(scale < 1){
        scale = 1;
    }
    FrameBufferText(x, y, text, (uint16_t)color, scale);
}

void syBasicGFXLine(int x1, int y1, int x2, int y2, int color){
    FrameBufferLine(x1, y1, x2, y2, (uint16_t)color);
}

void syBasicGFXFlush(void){
    FrameBufferFlush();
}

int basicInTouch(void){
    int touchX;
    int touchY;
    if(touchScreenGet(&touchX, &touchY)){
        lastTouchX = touchX;
        lastTouchY = touchY;
        return 1;
    }
    return 0;
}

int basicInTouchX(void){
    return lastTouchX;
}

int basicInTouchY(void){
    return lastTouchY;
}

void basicYield(void){
    syThreadYield();
}

uint32_t basicTicks(void){
    return syGetRealTimeSystemClock();
}

static int formatInteger(int value, char *buffer){
    int length = 0;
    unsigned int magnitude;
    if(value < 0){
        buffer[length++] = '-';
        magnitude = (unsigned int)(-(value + 1)) + 1u;
    } else {
        magnitude = (unsigned int)value;
    }

    char digits[12];
    int digitCount = 0;
    if(magnitude == 0){
        digits[digitCount++] = '0';
    }
    while(magnitude){
        digits[digitCount++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }
    while(digitCount > 0){
        buffer[length++] = digits[--digitCount];
    }
    buffer[length] = 0;
    return length;
}

void basicOutString(const char *text){
    uartPuts(text);
}

void basicOutInt(int value){
    char buffer[16];
    formatInteger(value, buffer);
    uartPuts(buffer);
}

void basicOutNewline(void){
    uartPuts("\n");
}