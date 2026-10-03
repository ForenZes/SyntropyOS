//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#ifndef BASIC_INTERNAL_H
#define BASIC_INTERNAL_H

#include "BasicApp.h"

#define BASIC_CODE_BYTES   (24 * 1024)
#define BASIC_NAME_LEN     8
#define BASIC_STR_SCRATCH  (4 * 1024)
#define BASIC_LINE_TMP     512

extern uint8_t  basicCode[BASIC_CODE_BYTES];
extern int      basicCodeTop;
extern basicLine basicProg[BASIC_MAX_LINES];
extern int      basicProgCount;

extern char basicNames[BASIC_MAX_NAMES][BASIC_NAME_LEN];
extern int  basicNameCount;

extern int basicErr;
extern int basicErrLine;

int  basicNameIndex(const char *s, int len);
int  basicFindLine(int number);

char *basicStrAlloc(int n);
void  basicStrReset(void);

int  readI32(const uint8_t *p);
void writeI32(uint8_t *p, int v);

void basicVarsReset(void);

void syBasicGFXClear(int color);
void syBasicGFXFill(int x, int y, int w, int h, int color);
void syBasicGFXBox(int x, int y, int w, int h, int color);
void syBasicGFXText(int x, int y, const char *s, int color, int scale);
void syBasicGFXLine(int x1, int y1, int x2, int y2, int color);
void syBasicGFXFlush(void);
int  basicInTouch(void);
int  basicInTouchX(void);
int  basicInTouchY(void);
void basicYield(void);
uint32_t basicTicks(void);
void basicOutString(const char *s);
void basicOutInt(int v);
void basicOutNewline(void);

#endif