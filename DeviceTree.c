//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include <stdint.h>
#include "Kernel.h"

int deviceType = 1;

#define EFUSE_BASE              0x3FF5A000UL
#define EFUSE_BLK0_RDATA1_REG   (EFUSE_BASE + 0x04)
#define EFUSE_BLK0_RDATA2_REG   (EFUSE_BASE + 0x08)

#define REG32(addr)             (*(volatile uint32_t *)(addr))

void syLLReadMACAddress(uint8_t *macAddy){
    uint32_t low  = REG32(EFUSE_BLK0_RDATA1_REG);
    uint32_t high = REG32(EFUSE_BLK0_RDATA2_REG);

    macAddy[0] = (high >> 8) & 0xFF;
    macAddy[1] = (high >> 0) & 0xFF;
    macAddy[2] = (low >> 24) & 0xFF;
    macAddy[3] = (low >> 16) & 0xFF;
    macAddy[4] = (low >> 8)  & 0xFF;
    macAddy[5] = (low >> 0)  & 0xFF;
}