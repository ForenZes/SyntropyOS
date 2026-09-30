//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#ifndef SETTINGSAPP_H
#define SETTINGSAPP_H

#include "Kernel.h"
#include "Framebuffer.h"
#include "TouchScreen.h"
#include "DiskManager.h"
#include "UserSpace.h"

void sySettingsAppRenderUI(void);
void initSettingsApp(void);
void touchScreenCalibrationApp(void);

#endif