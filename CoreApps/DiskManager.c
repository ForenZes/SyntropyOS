//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "DiskManager.h"
#include "Framebuffer.h"
#include "TouchScreen.h"

static const uint32_t ic_43[] = {
    0x0000,
    0x1FFE,
    0x1002,
    0x100A,
    0x17E2,
    0x1002,
    0x1FFE,
    0x0000,
    0x1FFE,
    0x1002,
    0x100A,
    0x17E2,
    0x1002,
    0x1FFE,
    0x0000,
    0x0000
};

static const uint32_t ic_46[] = {
    0x000E00,
    0x000700,
    0x000380,
    0x0001C0,
    0x3FFFE0,
    0x3FFFF0,
    0x3001E0,
    0x3003C0,
    0x300780,
    0x300F00,
    0x300E00,
    0x300000,
    0x300000,
    0x300000,
    0x300000,
    0x300000,
    0x300000,
    0x300000,
    0x3FFFFC,
    0x3FFFFC,
    0x000000,
    0x000000,
    0x000000,
    0x000000
};

static const uint32_t ic_50[] = {
    0x0000,
    0x0000,
    0x0FFE,
    0x0AAA,
    0x0AAA,
    0x1AAA,
    0x12AA,
    0x1002,
    0x1002,
    0x1002,
    0x1002,
    0x1002,
    0x1002,
    0x1002,
    0x1FFE,
    0x0000
};

static const char *sdTypeText(void){
    switch(sdCardGetType()){
        case SD_TYPE_SDHC: return "SDHC";
        case SD_TYPE_SD2:  return "SD v2";
        case SD_TYPE_SD1:  return "SD v1";
        default:           return "None";
    }
}

static const char *styleText(int style){
    if(style == 2){ return "GPT"; }
    if(style == 1){ return "MBR"; }
    return "None";
}

static void appendText(char *out, int *pos, const char *text){
    int i = 0;
    while(text[i] && *pos < 39){
        out[*pos] = text[i];
        (*pos)++;
        i++;
    }
    out[*pos] = 0;
}

static void appendUint(char *out, int *pos, uint32_t value){
    char tmp[12];
    int n = 0;
    if(value == 0){
        tmp[n++] = '0';
    } else {
        while(value){
            tmp[n++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    while(n > 0 && *pos < 39){
        out[*pos] = tmp[--n];
        (*pos)++;
    }
    out[*pos] = 0;
}

static void appendGigabytes(char *out, int *pos, uint32_t sectors){
    uint32_t tenths = sectors / 209715u;
    appendUint(out, pos, tenths / 10);
    appendText(out, pos, ".");
    appendUint(out, pos, tenths % 10);
    appendText(out, pos, " GB");
}

void DiskManagerUI(void){
    fatVolumeInfo vol;
    int have = fatGetVolumeInfo(&vol);

    FrameBufferClear(RGB(69, 69, 69));
    FrameBufferFillRect(0, 0, 320, 31, RGB(64, 118, 226));
    FrameBufferText(33, 8, "Disk Manager", RGB(255, 255, 255), 2);
    FrameBufferIcon(1, 0, ic_43, 14, 16, RGB(255, 255, 255), 2);
    FrameBufferFillRect(0, 212, 320, 28, RGB(64, 118, 226));
    FrameBufferIcon(4, 216, ic_46, 24, 24, RGB(255, 255, 255), 1);
    FrameBufferFillRect(0, 31, 320, 181, RGB(48, 48, 48));
    FrameBufferLine(0, 56, 119, 56, RGB(255, 255, 255));

    const char *diskLabel = "No Disk";
    if(have){
        diskLabel = (vol.fatType == 32) ? "Disk1 (FAT32)" : "Disk1 (FAT16)";
    }
    FrameBufferText(17, 40, diskLabel, RGB(255, 255, 255), 1);
    FrameBufferIcon(2, 34, ic_50, 16, 16, RGB(255, 255, 255), 1);

    FrameBufferRect(119, 35, 196, 172, RGB(255, 255, 255));
    FrameBufferBox(165, 172, 103, 25, RGB(64, 64, 68), 2, 1, RGB(255, 194, 194));
    FrameBufferText(171, 177, "Format", RGB(255, 194, 194), 2);

    char line[40];
    int pos;

    FrameBufferText(128, 41, have ? vol.label : "No Card", RGB(255, 255, 255), 1);

    pos = 0;
    appendText(line, &pos, "Partition Type: ");
    appendText(line, &pos, have ? (vol.fatType == 32 ? "FAT32" : "FAT16") : "-");
    FrameBufferText(128, 56, line, RGB(255, 255, 255), 1);

    pos = 0;
    appendText(line, &pos, "SD Card Type: ");
    appendText(line, &pos, sdTypeText());
    FrameBufferText(128, 71, line, RGB(255, 255, 255), 1);

    pos = 0;
    appendText(line, &pos, "Partition Style: ");
    appendText(line, &pos, have ? styleText(vol.partitionStyle) : "-");
    FrameBufferText(128, 87, line, RGB(255, 255, 255), 1);

    pos = 0;
    appendText(line, &pos, "Capacity: ");
    appendGigabytes(line, &pos, have ? vol.totalSectors : 0);
    FrameBufferText(128, 103, line, RGB(255, 255, 255), 1);

    uint32_t freeSectors = have ? vol.freeClusters * vol.sectorsPerCluster : 0;
    pos = 0;
    appendText(line, &pos, "Free Space: ");
    appendGigabytes(line, &pos, freeSectors);
    FrameBufferText(128, 118, line, RGB(255, 255, 255), 1);

    FrameBufferFillRect(129, 136, 174, 9, RGB(64, 118, 226));
    if(have && vol.dataClusters){
        uint32_t used = vol.dataClusters - vol.freeClusters;
        uint32_t usedWidth = (174u * used) / vol.dataClusters;
        if(usedWidth == 0 && used > 0){
            usedWidth = 1;
        }
        if(usedWidth > 174){
            usedWidth = 174;
        }
        if(usedWidth > 0){
            FrameBufferFillRect(129, 136, (int)usedWidth, 9, RGB(255, 107, 122));
        }
    }

    FrameBufferLine(0, 31, 319, 31, RGB(255, 255, 255));
    FrameBufferFlush();
}

static int inRect(int x, int y, int rx, int ry, int rw, int rh){
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void waitRelease(void){
    int x, y;
    while(touchScreenGet(&x, &y)){
    }
}

static void drawConfirm(void){
    FrameBufferFillRect(40, 78, 240, 92, RGB(48, 48, 52));
    FrameBufferRect(40, 78, 240, 92, RGB(255, 255, 255));
    FrameBufferText(60, 92, "Erase ALL data on the card?", RGB(255, 255, 255), 1);
    FrameBufferText(96, 108, "This cannot be undone.", RGB(255, 194, 194), 1);
    FrameBufferBox(54, 128, 90, 30, RGB(64, 64, 68), 3, 1, RGB(210, 210, 210));
    FrameBufferText(72, 135, "Cancel", RGB(255, 255, 255), 2);
    FrameBufferBox(176, 128, 90, 30, RGB(120, 40, 40), 3, 1, RGB(255, 194, 194));
    FrameBufferText(194, 135, "Erase", RGB(255, 194, 194), 2);
    FrameBufferFlush();
}

static void formatFlow(void){
    drawConfirm();

    while(0){
        int cx, cy;
        if(!touchScreenGet(&cx, &cy)){
            continue;
        }
        if(inRect(cx, cy, 54, 128, 90, 30)){
            waitRelease();
            DiskManagerUI();
            return;
        }
        if(inRect(cx, cy, 176, 128, 90, 30)){
            waitRelease();
            FrameBufferFillRect(40, 78, 240, 92, RGB(48, 48, 52));
            FrameBufferRect(40, 78, 240, 92, RGB(255, 255, 255));
            FrameBufferText(110, 118, "Formatting...", RGB(255, 255, 255), 2);
            FrameBufferFlush();

            int ok = fatFormat("SYNTROPY");
            fatMount();
            DiskManagerUI();

            FrameBufferText(108, 150, ok ? "Format complete" : "Format failed", ok ? RGB(160, 255, 160) : RGB(255, 120, 120), 1);
            FrameBufferFlush();
            return;
        }
    }
}

void diskManagerInit(void){
    DiskManagerUI();

    while(0){
        int x, y;
        if(touchScreenGet(&x, &y)){
            if(inRect(x, y, 165, 172, 103, 25)){
                waitRelease();
                formatFlow();
            } else {
                waitRelease();
            }
        }
        syThreadYield();
    }
}