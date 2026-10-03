//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>

typedef int coreStorage_t;

extern coreStorage_t coreStorageInitStatus;

typedef enum {
    SD_TYPE_NONE = 0,
    SD_TYPE_SD1,
    SD_TYPE_SD2,
    SD_TYPE_SDHC
} sdCardType_t;

#define FAT_LFN_MAX 63

typedef struct {
    char name[FAT_LFN_MAX + 1];
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

typedef struct {
    int present;
    sdCardType_t cardType;
    fatVolumeInfo volume;
} coreVolume_t;


int fatMount(void);
int fatListRoot(fatEntry *entries, int maxEntries);
int fatReadFile(const char *name, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize);
int fatListDir(const char *path, fatEntry *entries, int maxEntries);
int fatReadFileIn(const char *path, const char *name, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize);
int fatGetVolumeInfo(fatVolumeInfo *info);
int fatFormat(const char *label);

void coreStorageCacheVolume(void);
void coreStorageRefresh(void);
const coreVolume_t *coreStorageVolume(void);
int coreStorageInit(void);
int sdReadBlock(uint32_t blockNumber, uint8_t *destination512);
int sdWriteBlock(uint32_t blockNumber, const uint8_t *source512);

sdCardType_t sdCardGetType(void);

#endif