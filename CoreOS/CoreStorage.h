//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>

typedef enum {
    SD_TYPE_NONE = 0,
    SD_TYPE_SD1,
    SD_TYPE_SD2,
    SD_TYPE_SDHC
} sdCardType_t;

typedef struct {
    char name[13];
    uint32_t fileSize;
    uint32_t firstCluster;
    int isDirectory;
} fatEntry;
 
typedef struct {
    char label[12];
    int fatType;
    int partitionStyle;
    uint32_t totalSectors;
    uint32_t sectorsPerCluster;
    uint32_t dataClusters;
    uint32_t freeClusters;
} fatVolumeInfo;

int fatMount(void);
int fatListRoot(fatEntry *entries, int maxEntries);
int fatReadFile(const char *name, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize);
int fatGetVolumeInfo(fatVolumeInfo *info);
int fatFormat(const char *label);

int coreStorageInit(void);
int sdReadBlock(uint32_t blockNumber, uint8_t *destination512);
int sdWriteBlock(uint32_t blockNumber, const uint8_t *source512);

sdCardType_t sdCardGetType(void);

#endif