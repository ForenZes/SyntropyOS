//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 
#ifndef DEVICETREE_H
#define DEVICETREE_H

#include <stdint.h>
#include "Kernel.h"

extern int deviceType;
void syLLReadMACAddress(uint8_t *macAddy);
#endif