//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>

void initTouchScreen(void);
int touchScreenGetRaw(uint16_t *rx, uint16_t *ry);
int touchScreenGet(int *x, int *y);

// configurable through the touch screen cfg app in userland.
extern int TOUCH_MIN_X;
extern int TOUCH_MAX_X;
extern int TOUCH_MIN_Y;
extern int TOUCH_MAX_Y;
extern int TOUCH_SWAP_XY;
extern int TOUCH_FLIP_X;
extern int TOUCH_FLIP_Y;
extern int SCREEN_W;
extern int SCREEN_H;
#endif
