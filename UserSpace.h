//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include "Kernel.h"
#ifndef SWITCHBOARD_H
#define SWITCHBOARD_H

#include <stdint.h>

void switchboardDraw(const char *username, int wifiConnected);
void touchScreenCalibrationApp(void);
void initSyntropyUserSpace(void);
void *syntropyDesktopMonitor(void *arg);
extern bool SetupDoneAllSteps;
#endif
