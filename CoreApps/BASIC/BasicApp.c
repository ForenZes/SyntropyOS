//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "BasicApp.h"
#include "Framebuffer.h"
#include "TouchScreen.h"
#include "CoreStorage.h"
#include "UART.h"
#include "GraphicsCache.h"

#define APP_PATH     "syntropyOS/APPLICATIONS"
#define SYAPP_MAGIC  0x0BADBABEu
#define MAX_APPS     32
#define APP_NAME_MAX 40
#define APP_DESC_MAX 48
#define APP_FILEBUF  8192

#define ROW_TOP   38
#define ROW_H     31
#define ROW_VIS   5

typedef struct {
    char name[APP_NAME_MAX];
    char desc[APP_DESC_MAX];
    char file[FAT_LFN_MAX + 1];
    uint32_t icon[16];
} syAppBlob;

static syAppBlob appList[MAX_APPS];
static int appCount;
static uint8_t appFileBuf[APP_FILEBUF];

static const uint32_t hubTitleIcon[16] = {
    0xFFFF, 0x8001, 0xBE7D, 0xBE7D, 0xBE7D, 0xBE7D, 0xBE7D, 0x8001,
    0x8001, 0xBE7D, 0xBE7D, 0xBE7D, 0xBE7D, 0xBE7D, 0x8001, 0xFFFF
};

static const uint32_t exitIcon[] = {
    0x0000,
    0x0000,
    0x0080,
    0x00C0,
    0x00E0,
    0x00F0,
    0xFFF8,
    0xFFFC,
    0xFFFC,
    0xFFF8,
    0x00F0,
    0x00E0,
    0x00C0,
    0x0080,
    0x0000,
    0x0000
};

static int inRect(int x, int y, int rectX, int rectY, int rectWidth, int rectHeight){
    return x >= rectX && x < rectX + rectWidth && y >= rectY && y < rectY + rectHeight;
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

static int ciEndsWithSYApp(const char *name){
    int length = 0;
    while(name[length]){
        length++;
    }
    if(length < 6){
        return 0;
    }
    const char *tail = name + length - 6;
    char lower[6];
    for(int i = 0; i < 6; i++){
        char ch = tail[i];
        if(ch >= 'A' && ch <= 'Z'){
            ch += 32;
        }
        lower[i] = ch;
    }
    return lower[0] == '.' && lower[1] == 's' && lower[2] == 'y'
        && lower[3] == 'a' && lower[4] == 'p' && lower[5] == 'p';
}

static int nextLine(const uint8_t *buf, int size, int *position, char *out, int outMax){
    if(*position >= size){
        return 0;
    }
    int length = 0;
    while(*position < size){
        char ch = (char)buf[*position];
        (*position)++;
        if(ch == '\n'){
            break;
        }
        if(ch == '\r'){
            continue;
        }
        if(length < outMax - 1){
            out[length++] = ch;
        }
    }
    out[length] = 0;
    return 1;
}

static void trimTrailing(char *text){
    int length = 0;

    while(text[length]){
        length++;
    }

    while(length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t')){
        length--;
    }
    text[length] = 0;
}

static uint32_t parseHexRow(const char *text){
    while(*text == ' ' || *text == '\t'){
        text++;
    }
    if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X')){
        text += 2;
    }
    uint32_t value = 0;
    while(*text){
        char ch = *text;
        int digit;
        if(ch >= '0' && ch <= '9'){
            digit = ch - '0';
        } else if(ch >= 'a' && ch <= 'f'){
            digit = ch - 'a' + 10;
        } else if(ch >= 'A' && ch <= 'F'){
            digit = ch - 'A' + 10;
        } else {
            break;
        }
        value = (value << 4) | (uint32_t)digit;
        text++;
    }
    return value;
}

static int syParseSYAPPHeader(const uint8_t *buf, int size, syAppBlob *out, int *codeStart){
    int position = 0;
    char line[128];

    if(!nextLine(buf, size, &position, line, sizeof(line))){
        return 0;
    }
    if(parseHexRow(line) != SYAPP_MAGIC){
        return 0;
    }

    if(!nextLine(buf, size, &position, line, sizeof(line))){
        return 0;
    }

    trimTrailing(line);
    int writePos = 0;
    while(line[writePos] && writePos < APP_NAME_MAX - 1){
        out->name[writePos] = line[writePos];
        writePos++;
    }
    out->name[writePos] = 0;

    if(!nextLine(buf, size, &position, line, sizeof(line))){
        return 0;
    }

    trimTrailing(line);
    writePos = 0;
    while(line[writePos] && writePos < APP_DESC_MAX - 1){
        out->desc[writePos] = line[writePos];
        writePos++;
    }

    out->desc[writePos] = 0;

    for(int row = 0; row < 16; row++){
        if(!nextLine(buf, size, &position, line, sizeof(line))){
            return 0;
        }
        out->icon[row] = parseHexRow(line);
    }

    *codeStart = position;
    return 1;
}

static void syScanSYAPPS(void){
    appCount = 0;
    fatEntry entries[MAX_APPS];
    int entryCount = fatListDir(APP_PATH, entries, MAX_APPS);
    for(int entryIndex = 0; entryIndex < entryCount && appCount < MAX_APPS; entryIndex++){
        if(entries[entryIndex].isDirectory){
            continue;
        }

        // Does the file have the SYAPP extension?
        if(!ciEndsWithSYApp(entries[entryIndex].name)){
            continue;
        }

        uint32_t fileSize = 0;
        int bytesRead = fatReadFileIn(APP_PATH, entries[entryIndex].name, appFileBuf, APP_FILEBUF - 1, &fileSize);
        if(bytesRead <= 0){
            continue;
        }

        // Does the app have the 0xBADBABE header?
        syAppBlob *item = &appList[appCount];
        int codeStart;
        if(!syParseSYAPPHeader(appFileBuf, bytesRead, item, &codeStart)){
            continue;
        }

        // Then DO populate the hub with these...
        int copyPos = 0;
        while(entries[entryIndex].name[copyPos] && copyPos < FAT_LFN_MAX){
            item->file[copyPos] = entries[entryIndex].name[copyPos];
            copyPos++;
        }
        item->file[copyPos] = 0;

        appCount++;
    }
}

static void drawSYAPPHub(void){
    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferFillRect(0, 0, 320, 35, RGB(0, 168, 104));
    FrameBufferText(39, 9, "Applications", RGB(255, 255, 255), 2);
    FrameBufferIcon(2, 1, hubTitleIcon, 16, 16, RGB(255, 255, 255), 2);

    if(appCount == 0){
        FrameBufferText(20, 110, "No apps found!", RGB(230, 230, 230), 2);
    }

    int visibleCount = appCount < ROW_VIS ? appCount : ROW_VIS;
    for(int i = 0; i < visibleCount; i++){
        int rowY = ROW_TOP + i * ROW_H;
        FrameBufferFillRect(0, rowY, 320, 30, RGB(98, 157, 134));
        FrameBufferRect(0, rowY, 320, 30, RGB(255, 255, 255));
        FrameBufferLine(35, rowY + 1, 35, rowY + 28, RGB(2, 131, 81));
        FrameBufferIcon(2, rowY - 1, appList[i].icon, 16, 16, RGB(255, 255, 255), 2);
        FrameBufferText(38, rowY + 4, appList[i].name, RGB(255, 255, 255), 1);
        FrameBufferText(38, rowY + 17, appList[i].desc, RGB(224, 224, 224), 1);
    }

    FrameBufferFillRect(0, 209, 320, 31, RGB(0, 168, 104));
    FrameBufferIcon(4, 216, exitIcon, 16, 16, RGB(255, 255, 255), 1);
    FrameBufferText(24, 221, "EXIT", RGB(255, 255, 255), 1);
    FrameBufferRect(2, 212, 58, 25, RGB(255, 255, 255));
    FrameBufferFlush();
}

static void drawAppError(const char *message, int line){
    FrameBufferClear(RGB(40, 24, 24));
    FrameBufferFillRect(0, 0, 320, 31, RGB(160, 48, 48));
    FrameBufferText(10, 8, "App Error", RGB(255, 255, 255), 2);
    FrameBufferText(16, 60, message, RGB(255, 210, 210), 2);
    if(line > 0){
        char buffer[24];
        int writePos = 0;
        const char *prefix = "at line ";
        while(prefix[writePos]){
            buffer[writePos] = prefix[writePos];
            writePos++;
        }
        formatInteger(line, buffer + writePos);
        FrameBufferText(16, 90, buffer, RGB(255, 210, 210), 2);
    }
    FrameBufferText(16, 200, "Tap to return", RGB(220, 220, 220), 1);
    FrameBufferFlush();

    int touchX;
    int touchY;
    while(!touchScreenGet(&touchX, &touchY)){
        syThreadYield();
    }
    syTouchscreenWaitRelease();
}

static void syParseAndInitSYAPPBinary(syAppBlob *item){
    uint32_t fileSize = 0;
    int bytesRead = fatReadFileIn(APP_PATH, item->file, appFileBuf, APP_FILEBUF - 1, &fileSize);

    if(bytesRead <= 0){
        drawAppError("Cannot read app", 0);
        return;
    }

    syAppBlob header;
    int codeStart;
    if(!syParseSYAPPHeader(appFileBuf, bytesRead, &header, &codeStart)){
        drawAppError("Bad app file", 0);
        return;
    }

    basicReset();

    int position = codeStart;
    char line[256];

    while(nextLine(appFileBuf, bytesRead, &position, line, sizeof(line))){
        int isBlank = 1;
        for(int i = 0; line[i]; i++){
            if(line[i] != ' ' && line[i] != '\t'){
                isBlank = 0;
                break;
            }
        }
        if(isBlank){
            continue;
        }
        int enterResult = basicEnterLine(line);
        if(enterResult != BASIC_OK){
            drawAppError(basicErrorText(enterResult), 0);
            return;
        }
    }

    int runResult = initializeBASICApp();
    if(runResult != BASIC_OK){
        drawAppError(basicErrorText(runResult), basicErrorLine());
    }
}

void syAppHubInit(void){
    syScanSYAPPS();

    for(;;){
        drawSYAPPHub();

        int launched = 0;
        while(!launched){
            int touchX;
            int touchY;
            if(touchScreenGet(&touchX, &touchY)){
                if(inRect(touchX, touchY, 2, 212, 58, 25)){
                    syTouchscreenWaitRelease();
                    return;
                }
                int hitIndex = -1;
                int visibleCount = appCount < ROW_VIS ? appCount : ROW_VIS;
                for(int i = 0; i < visibleCount; i++){
                    int rowY = ROW_TOP + i * ROW_H;
                    if(inRect(touchX, touchY, 0, rowY, 320, 30)){
                        hitIndex = i;
                        break;
                    }
                }
                syTouchscreenWaitRelease();
                if(hitIndex >= 0){
                    syParseAndInitSYAPPBinary(&appList[hitIndex]);
                    launched = 1;
                }
            }
            syThreadYield();
        }
    }
}