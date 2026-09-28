#include "Console.h"
#include "Framebuffer.h"

#define LINE_GAP 4
#define CON_MARGIN 6

static int curX;
static int curY;
static uint16_t conFg;
static uint16_t conBg;
static int conScale;

static int glyphHeight(void){
    return 8 * conScale;
}

static int lineHeight(void){
    return 8 * conScale + LINE_GAP;
}

void syConsoleInit(uint16_t foregroundColor, uint16_t backgroundColor, int scale){
    if(scale < 1){
        scale = 1;
    }

    conFg = foregroundColor;
    conBg = backgroundColor;
    conScale = scale;
    curX = CON_MARGIN;
    curY = CON_MARGIN;
    FrameBufferClear(backgroundColor);
    FrameBufferFlush();
}

static void syConsoleScroll(void){
    int shift = lineHeight() * FB_WIDTH;
    int total = FB_WIDTH * FB_HEIGHT;

    for(int i = 0; i < total - shift; i++){
        SyntropyFrameBuffer[i] = SyntropyFrameBuffer[i + shift];
    }

    for(int i = total - shift; i < total; i++){
        SyntropyFrameBuffer[i] = conBg;
    }

    curY -= lineHeight();
    FrameBufferFlush();
}

void syConsolePutChar(char character){
    int cellW = 8 * conScale;

    if(character == '\r'){
        curX = CON_MARGIN;
        return;
    }

    if(character == '\n'){
        curX = CON_MARGIN;
        curY += lineHeight();
        if(curY + glyphHeight() > FB_HEIGHT - CON_MARGIN){
            syConsoleScroll();
        }
        return;
    }

    if(curX + cellW > FB_WIDTH - CON_MARGIN){
        curX = CON_MARGIN;
        curY += lineHeight();
        if(curY + glyphHeight() > FB_HEIGHT - CON_MARGIN){
            syConsoleScroll();
        }
    }

    FrameBufferChar(curX, curY, character, conFg, conScale);
    FrameBufferFlushRect(curX, curY, cellW, glyphHeight());
    curX += cellW;
}

void syConsolePutString(const char *string){
    while(*string){
        syConsolePutChar(*string);
        string++;
    }
}
