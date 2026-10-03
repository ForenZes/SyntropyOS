//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include "CoreStorage.h"
#include "Kernel.h"

coreStorage_t coreStorageInitStatus;
static coreVolume_t gVolume;

#define DR_REG_GPIO  0x3FF44000
#define DR_REG_IOMUX 0x3FF49000
#define DR_REG_DPORT 0x3FF00000
#define DR_REG_SPI3  0x3FF65000

#define GPIO_OUT_W1TS        (*(volatile uint32_t*)(DR_REG_GPIO + 0x08))
#define GPIO_OUT_W1TC        (*(volatile uint32_t*)(DR_REG_GPIO + 0x0C))
#define GPIO_ENABLE_W1TS     (*(volatile uint32_t*)(DR_REG_GPIO + 0x24))
#define GPIO_FUNC_OUT_SEL(n) (*(volatile uint32_t*)(DR_REG_GPIO + 0x530 + (n)*4))
#define GPIO_FUNC_IN_SEL(n)  (*(volatile uint32_t*)(DR_REG_GPIO + 0x130 + (n)*4))

#define IOMUX_GPIO5  (*(volatile uint32_t*)(DR_REG_IOMUX + 0x6C))
#define IOMUX_GPIO18 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x70))
#define IOMUX_GPIO19 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x74))
#define IOMUX_GPIO23 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x8C))

#define DPORT_PERIP_CLK_EN (*(volatile uint32_t*)(DR_REG_DPORT + 0xC0))
#define DPORT_PERIP_RST_EN (*(volatile uint32_t*)(DR_REG_DPORT + 0xC4))

#define SPI3_CMD       (*(volatile uint32_t*)(DR_REG_SPI3 + 0x00))
#define SPI3_CTRL      (*(volatile uint32_t*)(DR_REG_SPI3 + 0x08))
#define SPI3_CLOCK     (*(volatile uint32_t*)(DR_REG_SPI3 + 0x18))
#define SPI3_USER      (*(volatile uint32_t*)(DR_REG_SPI3 + 0x1C))
#define SPI3_USER1     (*(volatile uint32_t*)(DR_REG_SPI3 + 0x20))
#define SPI3_USER2     (*(volatile uint32_t*)(DR_REG_SPI3 + 0x24))
#define SPI3_MOSI_DLEN (*(volatile uint32_t*)(DR_REG_SPI3 + 0x28))
#define SPI3_MISO_DLEN (*(volatile uint32_t*)(DR_REG_SPI3 + 0x2C))
#define SPI3_PIN       (*(volatile uint32_t*)(DR_REG_SPI3 + 0x34))
#define SPI3_W0        ((volatile uint32_t*)(DR_REG_SPI3 + 0x80))

#define VSPICLK_SIGNAL 63
#define VSPIQ_SIGNAL   64
#define VSPID_SIGNAL   65

#define SD_PIN_CS   5
#define SD_PIN_SCK  18
#define SD_PIN_MOSI 23
#define SD_PIN_MISO 19

#define SPI_CLOCK_INIT 0x004C9109   // 200 kHz
#define SPI_CLOCK_FAST 0x000070C7   // 5 MHz

// FAT Shenanigans
static int fatType = 0;
static int mounted = 0;
static int partitionStyle = 0;
static uint32_t volumeSectors = 0;
static uint32_t dataClusters = 0;
 
static uint32_t partitionStart = 0;
static uint32_t sectorsPerCluster = 0;
static uint32_t reservedSectors = 0;
static uint32_t numFats = 0;
static uint32_t fatSize = 0;
static uint32_t rootCluster = 0;
static uint32_t rootDirStartSector = 0;
static uint32_t rootDirSectors = 0;
static uint32_t fatStartSector = 0;
static uint32_t dataStartSector = 0;
 
static uint8_t sectorBuffer[512];
 
static uint32_t walkCluster = 0;
static uint32_t walkSectorIndex = 0;
static uint32_t walkFixedIndex = 0;
static int walkDone = 0;
//

static sdCardType_t detectedType = SD_TYPE_NONE;
static int usesBlockAddressing = 0;

static void sdPinHigh(int pin){ GPIO_OUT_W1TS = (1u << pin); }
static void sdPinLow(int pin){ GPIO_OUT_W1TC = (1u << pin); }

static void sdBusSetup(void){
    DPORT_PERIP_CLK_EN |= (1u << 7);  // enable
    DPORT_PERIP_RST_EN &= ~(1u << 7); // take the spi out of reset

    IOMUX_GPIO18 = (2u << 12);
    IOMUX_GPIO23 = (2u << 12);
    IOMUX_GPIO19 = (2u << 12) | (1u << 9);

    GPIO_FUNC_OUT_SEL(SD_PIN_SCK)  = VSPICLK_SIGNAL;
    GPIO_FUNC_OUT_SEL(SD_PIN_MOSI) = VSPID_SIGNAL;
    GPIO_ENABLE_W1TS = (1u << SD_PIN_SCK) | (1u << SD_PIN_MOSI); // switch the pads to output mode
    GPIO_FUNC_IN_SEL(VSPIQ_SIGNAL) = (1u << 7) | SD_PIN_MISO;

    IOMUX_GPIO5 = (2u << 12);
    GPIO_FUNC_OUT_SEL(SD_PIN_CS) = 0x100;
    GPIO_ENABLE_W1TS = (1u << SD_PIN_CS);
    sdPinHigh(SD_PIN_CS);

    SPI3_CTRL  = 0;
    SPI3_USER  = (1u << 0) | (1u << 27) | (1u << 28);
    SPI3_USER1 = 0;
    SPI3_USER2 = 0;
    SPI3_PIN   = 0;
    SPI3_CLOCK = SPI_CLOCK_INIT;
}

static uint8_t sdTransferByte(uint8_t outgoing){
    SPI3_W0[0] = outgoing;
    SPI3_MOSI_DLEN = 7;
    SPI3_MISO_DLEN = 7;
    SPI3_CMD = (1u << 18);
    while(SPI3_CMD & (1u << 18)){
    }
    return (uint8_t)(SPI3_W0[0] & 0xFF);
}

static uint8_t sdSendCommand(uint8_t index, uint32_t argument){
    sdTransferByte(0x40 | index);
    sdTransferByte((uint8_t)(argument >> 24));
    sdTransferByte((uint8_t)(argument >> 16));
    sdTransferByte((uint8_t)(argument >> 8));
    sdTransferByte((uint8_t)argument);

    uint8_t crc = 0xFF;
    if(index == 0){ crc = 0x95; }
    if(index == 8){ crc = 0x87; }
    sdTransferByte(crc);

    uint8_t response = 0xFF;
    for(int attempt = 0; attempt < 8; attempt++){
        response = sdTransferByte(0xFF);
        if((response & 0x80) == 0){ break; }
    }
    return response;
}

int coreStorageInit(void){
    detectedType = SD_TYPE_NONE;
    usesBlockAddressing = 0;
    sdBusSetup();
    syWaitMilliseconds(2);

    sdPinHigh(SD_PIN_CS);
    for(int i = 0; i < 12; i++){ sdTransferByte(0xFF); }
    sdPinLow(SD_PIN_CS);

    uint8_t r1 = 0xFF;
    for(int attempt = 0; attempt < 10; attempt++){
        r1 = sdSendCommand(0, 0);
        if(r1 == 0x01){ break; }
        syWaitMilliseconds(10);
    }
    if(r1 != 0x01){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    int isVersion2 = 0;
    r1 = sdSendCommand(8, 0x000001AA);
    if(r1 == 0x01){
        sdTransferByte(0xFF);
        sdTransferByte(0xFF);
        uint8_t voltageEcho = sdTransferByte(0xFF);
        uint8_t patternEcho = sdTransferByte(0xFF);
        if(voltageEcho == 0x01 && patternEcho == 0xAA){ isVersion2 = 1; }
    }

    uint32_t initArgument = isVersion2 ? 0x40000000u : 0u;
    for(int attempt = 0; attempt < 2000; attempt++){
        sdSendCommand(55, 0);
        r1 = sdSendCommand(41, initArgument);
        if(r1 == 0x00){ break; }
        syWaitMilliseconds(2);
    }
    if(r1 != 0x00){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    if(isVersion2){
        r1 = sdSendCommand(58, 0);
        if(r1 == 0x00){
            uint8_t ocrHigh = sdTransferByte(0xFF);
            sdTransferByte(0xFF);
            sdTransferByte(0xFF);
            sdTransferByte(0xFF);
            if(ocrHigh & 0x40){ usesBlockAddressing = 1; }
        }
    }

    if(!usesBlockAddressing){
        sdSendCommand(16, 512);
    }

    sdPinHigh(SD_PIN_CS);
    sdTransferByte(0xFF);

    SPI3_CLOCK = SPI_CLOCK_FAST;

    if(usesBlockAddressing){ detectedType = SD_TYPE_SDHC; }
    else if(isVersion2){ detectedType = SD_TYPE_SD2; }
    else { detectedType = SD_TYPE_SD1; }
    return 1;
}

int sdReadBlock(uint32_t blockNumber, uint8_t *destination512){
    uint32_t address = usesBlockAddressing ? blockNumber : (blockNumber * 512u);

    sdPinLow(SD_PIN_CS);
    if(sdSendCommand(17, address) != 0x00){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    uint8_t token = 0xFF;
    for(int wait = 0; wait < 40000; wait++){
        token = sdTransferByte(0xFF);
        if(token != 0xFF){ break; }
    }
    if(token != 0xFE){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    for(int i = 0; i < 512; i++){
        destination512[i] = sdTransferByte(0xFF);
    }
    sdTransferByte(0xFF);
    sdTransferByte(0xFF);

    sdPinHigh(SD_PIN_CS);
    sdTransferByte(0xFF);
    return 1;
}

int sdWriteBlock(uint32_t blockNumber, const uint8_t *source512){
    uint32_t address = usesBlockAddressing ? blockNumber : (blockNumber * 512u);

    sdPinLow(SD_PIN_CS);
    if(sdSendCommand(24, address) != 0x00){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    sdTransferByte(0xFF);
    sdTransferByte(0xFE);
    for(int i = 0; i < 512; i++){
        sdTransferByte(source512[i]);
    }
    sdTransferByte(0xFF);
    sdTransferByte(0xFF);

    uint8_t dataResponse = sdTransferByte(0xFF);
    if((dataResponse & 0x1F) != 0x05){
        sdPinHigh(SD_PIN_CS);
        return 0;
    }

    for(int wait = 0; wait < 200000; wait++){
        if(sdTransferByte(0xFF) == 0xFF){ break; }
    }

    sdPinHigh(SD_PIN_CS);
    sdTransferByte(0xFF);
    return 1;
}

sdCardType_t sdCardGetType(void){
    return detectedType;
}

// FAT16, FAT32 Support

static uint16_t readLe16(const uint8_t *b, int offset){
    return (uint16_t)(b[offset] | (b[offset + 1] << 8));
}
 
static uint32_t readLe32(const uint8_t *b, int offset){
    return (uint32_t)b[offset]
         | ((uint32_t)b[offset + 1] << 8)
         | ((uint32_t)b[offset + 2] << 16)
         | ((uint32_t)b[offset + 3] << 24);
}
 
static void writeLe16(uint8_t *b, int offset, uint16_t v){
    b[offset] = (uint8_t)(v & 0xFF);
    b[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
}
 
static void writeLe32(uint8_t *b, int offset, uint32_t v){
    b[offset] = (uint8_t)(v & 0xFF);
    b[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
    b[offset + 2] = (uint8_t)((v >> 16) & 0xFF);
    b[offset + 3] = (uint8_t)((v >> 24) & 0xFF);
}
 
static char upcase(char c){
    if(c >= 'a' && c <= 'z'){
        return (char)(c - 32);
    }
    return c;
}
 
static int clusterIsEnd(uint32_t cluster){
    if(fatType == 32){
        return cluster >= 0x0FFFFFF8u;
    }
    return cluster >= 0xFFF8u;
}
 
static uint32_t clusterFirstSector(uint32_t cluster){
    return dataStartSector + (cluster - 2) * sectorsPerCluster;
}
 
static uint32_t fatNextCluster(uint32_t cluster){
    if(fatType == 32){
        uint32_t fatOffset = cluster * 4;
        uint32_t sector = fatStartSector + (fatOffset / 512);
        uint32_t within = fatOffset % 512;
        if(!sdReadBlock(sector, sectorBuffer)){
            return 0x0FFFFFFFu;
        }
        return readLe32(sectorBuffer, within) & 0x0FFFFFFFu;
    }
 
    uint32_t fatOffset = cluster * 2;
    uint32_t sector = fatStartSector + (fatOffset / 512);
    uint32_t within = fatOffset % 512;
    if(!sdReadBlock(sector, sectorBuffer)){
        return 0xFFFFu;
    }
    return readLe16(sectorBuffer, within);
}
 
static void makeShortName(const char *input, char *out11){
    for(int i = 0; i < 11; i++){ out11[i] = ' '; }
 
    int i = 0;
    int o = 0;
    while(input[i] && input[i] != '.' && o < 8){
        char c = input[i];
        if(c >= 'a' && c <= 'z'){ c -= 32; }
        out11[o++] = c;
        i++;
    }
    while(input[i] && input[i] != '.'){ i++; }
    if(input[i] == '.'){
        i++;
        int e = 8;
        while(input[i] && e < 11){
            char c = input[i];
            if(c >= 'a' && c <= 'z'){ c -= 32; }
            out11[e++] = c;
            i++;
        }
    }
}
 
static void shortNameToText(const uint8_t *raw, char *out){
    int o = 0;
    for(int i = 0; i < 8; i++){
        if(raw[i] != ' '){ out[o++] = (char)raw[i]; }
    }
    int hasExt = 0;
    for(int i = 8; i < 11; i++){
        if(raw[i] != ' '){ hasExt = 1; }
    }
    if(hasExt){
        out[o++] = '.';
        for(int i = 8; i < 11; i++){
            if(raw[i] != ' '){ out[o++] = (char)raw[i]; }
        }
    }
    out[o] = 0;
}
 
static void fillEntry(const uint8_t *raw, fatEntry *out){
    uint32_t high = readLe16(raw, 20);
    uint32_t low = readLe16(raw, 26);
    out->firstCluster = (high << 16) | low;
    out->fileSize = readLe32(raw, 28);
    out->isDirectory = (raw[11] & 0x10) ? 1 : 0;
    shortNameToText(raw, out->name);
}
 
static int isFatPartitionType(uint8_t t){
    return t == 0x01 || t == 0x04 || t == 0x06
        || t == 0x0B || t == 0x0C || t == 0x0E;
}
 
static uint32_t gptFirstPartitionLba(void){
    if(!sdReadBlock(1, sectorBuffer)){
        return 0;
    }
    if(sectorBuffer[0] != 'E' || sectorBuffer[1] != 'F'
    || sectorBuffer[2] != 'I' || sectorBuffer[3] != ' '){
        return 0;
    }
 
    uint32_t entryLba = readLe32(sectorBuffer, 72);
    uint32_t entryCount = readLe32(sectorBuffer, 80);
    uint32_t entrySize = readLe32(sectorBuffer, 84);
    if(entrySize == 0 || entrySize > 512){
        return 0;
    }
    uint32_t perSector = 512 / entrySize;
    if(perSector == 0){
        return 0;
    }
 
    for(uint32_t idx = 0; idx < entryCount; idx++){
        if(idx % perSector == 0){
            if(!sdReadBlock(entryLba + idx / perSector, sectorBuffer)){
                return 0;
            }
        }
        uint32_t off = (idx % perSector) * entrySize;
        int nonzero = 0;
        for(int k = 0; k < 16; k++){
            if(sectorBuffer[off + k]){
                nonzero = 1;
                break;
            }
        }
        if(nonzero){
            return readLe32(sectorBuffer, off + 32);
        }
    }
    return 0;
}
 
static int looksLikeBpb(void){
    if(readLe16(sectorBuffer, 11) != 512){
        return 0;
    }
    uint8_t spc = sectorBuffer[13];
    if(spc == 0 || (spc & (spc - 1))){
        return 0;
    }
    if(readLe16(sectorBuffer, 14) == 0){
        return 0;
    }
    if(sectorBuffer[16] == 0){
        return 0;
    }
    return 1;
}
 
static void dirWalkStart(void){
    if(fatType == 32){
        walkCluster = rootCluster;
        walkSectorIndex = 0;
    } else {
        walkFixedIndex = 0;
    }
    walkDone = 0;
}
 
static int dirWalkNext(uint32_t *sectorOut){
    if(walkDone){
        return 0;
    }
 
    if(fatType == 32){
        if(walkCluster < 2 || clusterIsEnd(walkCluster)){
            walkDone = 1;
            return 0;
        }
        *sectorOut = clusterFirstSector(walkCluster) + walkSectorIndex;
        walkSectorIndex++;
        if(walkSectorIndex >= sectorsPerCluster){
            walkSectorIndex = 0;
            walkCluster = fatNextCluster(walkCluster);
        }
        return 1;
    }
 
    if(walkFixedIndex >= rootDirSectors){
        walkDone = 1;
        return 0;
    }
    *sectorOut = rootDirStartSector + walkFixedIndex;
    walkFixedIndex++;
    return 1;
}
 
static void lfnExtract(const uint8_t *e, char *lfn, int posbase){
    static const int off[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    for(int k = 0; k < 13; k++){
        int p = posbase + k;
        if(p < 0 || p >= FAT_LFN_MAX){ continue; }
        uint16_t u = (uint16_t)(e[off[k]] | (e[off[k] + 1] << 8));
        if(u == 0x0000 || u == 0xFFFF){ continue; }
        lfn[p] = (u < 0x80) ? (char)u : '?';
    }
}

static int nameEqualCI(const char *a, const char *b){
    int i = 0;
    while(a[i] && b[i]){
        int ca = a[i], cb = b[i];
        if(ca >= 'a' && ca <= 'z'){ ca -= 32; }
        if(cb >= 'a' && cb <= 'z'){ cb -= 32; }
        if(ca != cb){ return 0; }
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static void copyName(char *dst, const char *src){
    int i = 0;
    while(src[i] && i < FAT_LFN_MAX){ dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int dirScan(uint32_t startCluster, int fixedRoot, const char *wantName, fatEntry *out, fatEntry *list, int maxList){
    char lfn[FAT_LFN_MAX + 1];
    int haveLfn = 0;
    int count = 0;

    uint32_t cluster = startCluster;
    uint32_t sub = 0;
    uint32_t fixedIndex = 0;

    for(;;){
        uint32_t sector;
        if(fixedRoot){
            if(fixedIndex >= rootDirSectors){ break; }
            sector = rootDirStartSector + fixedIndex;
            fixedIndex++;
        } else {
            if(cluster < 2 || clusterIsEnd(cluster)){ break; }
            sector = clusterFirstSector(cluster) + sub;
            sub++;
            if(sub >= sectorsPerCluster){ sub = 0; cluster = fatNextCluster(cluster); }
        }

        if(!sdReadBlock(sector, sectorBuffer)){ return wantName ? 0 : count; }

        for(int e = 0; e < 512; e += 32){
            uint8_t first = sectorBuffer[e];
            if(first == 0x00){ return wantName ? 0 : count; }
            if(first == 0xE5){ haveLfn = 0; continue; }

            uint8_t attr = sectorBuffer[e + 11];
            if(attr == 0x0F){
                int seq = first & 0x1F;
                if(first & 0x40){
                    for(int z = 0; z <= FAT_LFN_MAX; z++){ lfn[z] = 0; }
                }
                if(seq >= 1){ lfnExtract(&sectorBuffer[e], lfn, (seq - 1) * 13); }
                haveLfn = 1;
                continue;
            }
            if(attr & 0x08){ haveLfn = 0; continue; }

            char disp[FAT_LFN_MAX + 1];
            if(haveLfn){ copyName(disp, lfn); }
            else { shortNameToText(&sectorBuffer[e], disp); }
            haveLfn = 0;

            if(wantName){
                if(nameEqualCI(disp, wantName)){
                    fillEntry(&sectorBuffer[e], out);
                    copyName(out->name, disp);
                    return 1;
                }
            } else {
                if(count >= maxList){ return count; }
                fillEntry(&sectorBuffer[e], &list[count]);
                copyName(list[count].name, disp);
                count++;
            }
        }
    }
    return wantName ? 0 : count;
}

static int dirScanRoot(const char *wantName, fatEntry *out, fatEntry *list, int maxList){
    if(fatType == 32){ return dirScan(rootCluster, 0, wantName, out, list, maxList); }
    return dirScan(0, 1, wantName, out, list, maxList);
}

static int readEntryData(const fatEntry *found, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize){
    if(outSize){ *outSize = found->fileSize; }
    if(found->isDirectory){ return -1; }
    uint32_t remaining = found->fileSize;
    if(remaining > maxBytes){ remaining = maxBytes; }
    uint32_t copied = 0;
    uint32_t cluster = found->firstCluster;
    while(cluster >= 2 && !clusterIsEnd(cluster) && copied < remaining){
        uint32_t sector = clusterFirstSector(cluster);
        for(uint32_t s = 0; s < sectorsPerCluster && copied < remaining; s++){
            if(!sdReadBlock(sector + s, sectorBuffer)){ return (int)copied; }
            uint32_t chunk = remaining - copied;
            if(chunk > 512){ chunk = 512; }
            for(uint32_t i = 0; i < chunk; i++){ dest[copied + i] = sectorBuffer[i]; }
            copied += chunk;
        }
        cluster = fatNextCluster(cluster);
    }
    return (int)copied;
}

static int resolveDirCluster(const char *path, uint32_t *clusterOut, int *isRootOut){
    while(*path == '/'){ path++; }
    if(*path == 0){ *isRootOut = 1; *clusterOut = 0; return 1; }

    int atRoot = 1;
    uint32_t cluster = 0;
    char comp[FAT_LFN_MAX + 1];

    while(*path){
        int n = 0;
        while(*path && *path != '/'){
            if(n < FAT_LFN_MAX){ comp[n++] = *path; }
            path++;
        }
        comp[n] = 0;
        while(*path == '/'){ path++; }
        if(n == 0){ continue; }

        fatEntry found;
        int ok = atRoot ? dirScanRoot(comp, &found, 0, 0) : dirScan(cluster, 0, comp, &found, 0, 0);
        if(!ok || !found.isDirectory){ return 0; }
        cluster = found.firstCluster;
        atRoot = 0;
    }
    *isRootOut = 0;
    *clusterOut = cluster;
    return 1;
}
 
int fatMount(void){
    mounted = 0;
    partitionStyle = 0;
 
    if(!sdReadBlock(0, sectorBuffer)){
        return 0;
    }
    if(sectorBuffer[510] != 0x55 || sectorBuffer[511] != 0xAA){
        return 0;
    }
 
    uint32_t partStart = 0;
    if(!looksLikeBpb()){
        if(sectorBuffer[446 + 4] == 0xEE){
            partitionStyle = 2;
            partStart = gptFirstPartitionLba();
            if(partStart == 0){
                return 0;
            }
        } else {
            partitionStyle = 1;
            int found = 0;
            for(int p = 0; p < 4; p++){
                int base = 446 + p * 16;
                uint8_t type = sectorBuffer[base + 4];
                uint32_t lba = readLe32(sectorBuffer, base + 8);
                if(isFatPartitionType(type) && lba != 0){
                    partStart = lba;
                    found = 1;
                    break;
                }
            }
            if(!found){
                return 0;
            }
        }
        if(!sdReadBlock(partStart, sectorBuffer)){
            return 0;
        }
        if(sectorBuffer[510] != 0x55 || sectorBuffer[511] != 0xAA){
            return 0;
        }
        if(!looksLikeBpb()){
            return 0;
        }
    }
    sectorsPerCluster = sectorBuffer[13];
    reservedSectors = readLe16(sectorBuffer, 14);
    numFats = sectorBuffer[16];
 
    uint32_t rootEntryCount = readLe16(sectorBuffer, 17);
    uint32_t fatSize16 = readLe16(sectorBuffer, 22);
    uint32_t fatSize32 = readLe32(sectorBuffer, 36);
    uint32_t totalSectors16 = readLe16(sectorBuffer, 19);
    uint32_t totalSectors32 = readLe32(sectorBuffer, 32);
 
    fatSize = fatSize16 ? fatSize16 : fatSize32;
    uint32_t totalSectors = totalSectors16 ? totalSectors16 : totalSectors32;
 
    if(sectorsPerCluster == 0 || fatSize == 0 || totalSectors == 0){
        return 0;
    }
 
    rootDirSectors = (rootEntryCount * 32 + 511) / 512;
    fatStartSector = partStart + reservedSectors;
    rootDirStartSector = fatStartSector + numFats * fatSize;
    dataStartSector = rootDirStartSector + rootDirSectors;
 
    uint32_t dataSectors = totalSectors - (reservedSectors + numFats * fatSize + rootDirSectors);
    uint32_t countClusters = dataSectors / sectorsPerCluster;
 
    if(countClusters < 65525){
        fatType = 16;
    } else {
        fatType = 32;
        rootCluster = readLe32(sectorBuffer, 44);
        if(rootCluster < 2){
            return 0;
        }
    }
 
    partitionStart = partStart;
    volumeSectors = totalSectors;
    dataClusters = countClusters;
    mounted = 1;
    return 1;
}
 
static uint32_t fatCountFree(void){
    uint32_t freeCount = 0;
    uint32_t width = (fatType == 32) ? 4 : 2;
    uint32_t lastSector = 0xFFFFFFFFu;
 
    for(uint32_t c = 2; c < dataClusters + 2; c++){
        uint32_t off = c * width;
        uint32_t sec = fatStartSector + off / 512;
        uint32_t within = off % 512;
        if(sec != lastSector){
            if(!sdReadBlock(sec, sectorBuffer)){
                return freeCount;
            }
            lastSector = sec;
        }
        uint32_t val;
        if(width == 4){
            val = readLe32(sectorBuffer, within) & 0x0FFFFFFFu;
        } else {
            val = readLe16(sectorBuffer, within);
        }
        if(val == 0){
            freeCount++;
        }
    }
    return freeCount;
}
 
static void fatReadLabel(char *out){
    uint32_t sector;
    dirWalkStart();
    while(dirWalkNext(&sector)){
        if(!sdReadBlock(sector, sectorBuffer)){
            break;
        }
        for(int e = 0; e < 512; e += 32){
            uint8_t first = sectorBuffer[e];
            if(first == 0x00){
                goto fallback;
            }
            if(first == 0xE5){
                continue;
            }
            uint8_t attr = sectorBuffer[e + 11];
            if(attr == 0x0F){
                continue;
            }
            if(attr & 0x08){
                int len = 11;
                for(int i = 0; i < 11; i++){
                    out[i] = (char)sectorBuffer[e + i];
                }
                while(len > 0 && out[len - 1] == ' '){
                    len--;
                }
                out[len] = 0;
                return;
            }
        }
    }
fallback:
    out[0] = 'N'; out[1] = 'O'; out[2] = ' ';
    out[3] = 'N'; out[4] = 'A'; out[5] = 'M'; out[6] = 'E';
    out[7] = 0;
}
 
int fatGetVolumeInfo(fatVolumeInfo *info){
    if(!mounted){
        return 0;
    }
    fatReadLabel(info->label);
    info->fatType = fatType;
    info->partitionStyle = partitionStyle;
    info->totalSectors = volumeSectors;
    info->sectorsPerCluster = sectorsPerCluster;
    info->dataClusters = dataClusters;
    info->freeClusters = fatCountFree();
    return 1;
}
 
int fatListRoot(fatEntry *entries, int maxEntries){
    return dirScanRoot(0, 0, entries, maxEntries);
}

int fatReadFile(const char *name, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize){
    fatEntry found;
    if(!dirScanRoot(name, &found, 0, 0)){ return -1; }
    return readEntryData(&found, dest, maxBytes, outSize);
}

int fatListDir(const char *path, fatEntry *entries, int maxEntries){
    if(!mounted){ return 0; }
    uint32_t cluster;
    int isRoot;
    if(!resolveDirCluster(path, &cluster, &isRoot)){ return 0; }
    if(isRoot){ return dirScanRoot(0, 0, entries, maxEntries); }
    return dirScan(cluster, 0, 0, 0, entries, maxEntries);
}

int fatReadFileIn(const char *path, const char *name, uint8_t *dest, uint32_t maxBytes, uint32_t *outSize){
    if(!mounted){ return -1; }
    uint32_t cluster;
    int isRoot;
    if(!resolveDirCluster(path, &cluster, &isRoot)){ return -1; }
    fatEntry found;
    int ok = isRoot ? dirScanRoot(name, &found, 0, 0) : dirScan(cluster, 0, name, &found, 0, 0);
    if(!ok){ return -1; }
    return readEntryData(&found, dest, maxBytes, outSize);
}

static int resolvePartition(uint32_t *startOut, uint32_t *countOut){
    if(!sdReadBlock(0, sectorBuffer)){
        return 0;
    }
    if(sectorBuffer[510] != 0x55 || sectorBuffer[511] != 0xAA){
        return 0;
    }
 
    if(looksLikeBpb()){
        uint32_t t16 = readLe16(sectorBuffer, 19);
        uint32_t t32 = readLe32(sectorBuffer, 32);
        *startOut = 0;
        *countOut = t16 ? t16 : t32;
        return *countOut != 0;
    }
 
    if(sectorBuffer[446 + 4] == 0xEE){
        if(!sdReadBlock(1, sectorBuffer)){
            return 0;
        }
        if(sectorBuffer[0] != 'E' || sectorBuffer[1] != 'F'
        || sectorBuffer[2] != 'I' || sectorBuffer[3] != ' '){
            return 0;
        }
        uint32_t entryLba = readLe32(sectorBuffer, 72);
        uint32_t entryCount = readLe32(sectorBuffer, 80);
        uint32_t entrySize = readLe32(sectorBuffer, 84);
        if(entrySize == 0 || entrySize > 512){
            return 0;
        }
        uint32_t perSector = 512 / entrySize;
        if(perSector == 0){
            return 0;
        }
        for(uint32_t idx = 0; idx < entryCount; idx++){
            if(idx % perSector == 0){
                if(!sdReadBlock(entryLba + idx / perSector, sectorBuffer)){
                    return 0;
                }
            }
            uint32_t off = (idx % perSector) * entrySize;
            int nonzero = 0;
            for(int k = 0; k < 16; k++){
                if(sectorBuffer[off + k]){
                    nonzero = 1;
                    break;
                }
            }
            if(nonzero){
                uint32_t first = readLe32(sectorBuffer, off + 32);
                uint32_t last = readLe32(sectorBuffer, off + 40);
                *startOut = first;
                *countOut = (last >= first) ? (last - first + 1) : 0;
                return *countOut != 0;
            }
        }
        return 0;
    }
 
    for(int p = 0; p < 4; p++){
        int base = 446 + p * 16;
        uint8_t type = sectorBuffer[base + 4];
        uint32_t lba = readLe32(sectorBuffer, base + 8);
        uint32_t cnt = readLe32(sectorBuffer, base + 12);
        if(isFatPartitionType(type) && lba != 0 && cnt != 0){
            *startOut = lba;
            *countOut = cnt;
            return 1;
        }
    }
    return 0;
}
 
static void writeLabelEntry(uint8_t *b, const char *label){
    for(int i = 0; i < 11; i++){
        b[i] = ' ';
    }
    for(int i = 0; i < 11 && label[i]; i++){
        b[i] = (uint8_t)upcase(label[i]);
    }
    b[11] = 0x08;
}
 
int fatFormat(const char *label){
    uint32_t start = 0;
    uint32_t count = 0;
    if(!resolvePartition(&start, &count)){
        return 0;
    }
 
    uint32_t bps = 512;
    uint32_t numFats = 2;
    int type;
    uint32_t spc;
    uint32_t reserved;
    uint32_t rootEntries;
    uint32_t rootDirSecs;
    uint32_t fatSecs = 0;
    uint32_t clusters = 0;
 
    if(count <= 4194304u){
        type = 16;
        reserved = 1;
        rootEntries = 512;
        rootDirSecs = (rootEntries * 32 + bps - 1) / bps;
        spc = 1;
        for(;;){
            uint32_t tmp1 = count - (reserved + rootDirSecs);
            uint32_t tmp2 = 256 * spc + numFats;
            fatSecs = (tmp1 + (tmp2 - 1)) / tmp2;
            uint32_t dataSecs = count - (reserved + numFats * fatSecs + rootDirSecs);
            clusters = dataSecs / spc;
            if(clusters < 65525 && clusters >= 4085){
                break;
            }
            if(spc >= 64){
                break;
            }
            spc <<= 1;
        }
    } else {
        type = 32;
        reserved = 32;
        rootEntries = 0;
        rootDirSecs = 0;
        spc = 8;
        for(;;){
            uint32_t tmp1 = count - reserved;
            uint32_t tmp2 = (256 * spc + numFats) / 2;
            fatSecs = (tmp1 + (tmp2 - 1)) / tmp2;
            uint32_t dataSecs = count - (reserved + numFats * fatSecs);
            clusters = dataSecs / spc;
            if(clusters <= 0x0FFFFFF0u){
                break;
            }
            if(spc >= 128){
                break;
            }
            spc <<= 1;
        }
    }
 
    uint32_t fatStart = start + reserved;
    uint32_t rootStart = fatStart + numFats * fatSecs;
    uint32_t dataStart = rootStart + rootDirSecs;
 
    uint8_t *b = sectorBuffer;
    for(int i = 0; i < 512; i++){ 
        b[i] = 0; 
    }
 
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
    const char *oem = "SYNTROPY";
    for(int i = 0; i < 8; i++){ b[3 + i] = (uint8_t)oem[i]; }
 
    writeLe16(b, 11, (uint16_t)bps);
    b[13] = (uint8_t)spc;
    writeLe16(b, 14, (uint16_t)reserved);
    b[16] = (uint8_t)numFats;
    writeLe16(b, 17, (uint16_t)rootEntries);
    b[21] = 0xF8;
    writeLe16(b, 24, 63);
    writeLe16(b, 26, 255);
    writeLe32(b, 28, start);
 
    if(count < 0x10000u){
        writeLe16(b, 19, (uint16_t)count);
    } else {
        writeLe16(b, 19, 0);
        writeLe32(b, 32, count);
    }
 
    if(type == 16){
        writeLe16(b, 22, (uint16_t)fatSecs);
        b[36] = 0x80;
        b[38] = 0x29;
        writeLe32(b, 39, 0x53594E54u);
        for(int i = 0; i < 11; i++){ b[43 + i] = ' '; }
        if(label){
            for(int i = 0; i < 11 && label[i]; i++){ b[43 + i] = (uint8_t)upcase(label[i]); }
        }
        const char *fs = "FAT16   ";
        for(int i = 0; i < 8; i++){ b[54 + i] = (uint8_t)fs[i]; }
    } else {
        writeLe16(b, 22, 0);
        writeLe32(b, 36, fatSecs);
        writeLe32(b, 44, 2);
        writeLe16(b, 48, 1);
        writeLe16(b, 50, 6);
        b[64] = 0x80;
        b[66] = 0x29;
        writeLe32(b, 67, 0x53594E54u);
        for(int i = 0; i < 11; i++){ b[71 + i] = ' '; }
        if(label){
            for(int i = 0; i < 11 && label[i]; i++){ b[71 + i] = (uint8_t)upcase(label[i]); }
        }
        const char *fs = "FAT32   ";
        for(int i = 0; i < 8; i++){ b[82 + i] = (uint8_t)fs[i]; }
    }
 
    b[510] = 0x55;
    b[511] = 0xAA;
    if(!sdWriteBlock(start, b)){
        return 0;
    }
    if(type == 32){
        if(!sdWriteBlock(start + 6, b)){
            return 0;
        }
    }
 
    if(type == 32){
        for(int i = 0; i < 512; i++){ b[i] = 0; }
        writeLe32(b, 0, 0x41615252u);
        writeLe32(b, 484, 0x61417272u);
        writeLe32(b, 488, 0xFFFFFFFFu);
        writeLe32(b, 492, 0xFFFFFFFFu);
        b[510] = 0x55;
        b[511] = 0xAA;
        sdWriteBlock(start + 1, b);
        sdWriteBlock(start + 7, b);
    }
 
    for(int i = 0; i < 512; i++){ b[i] = 0; }
    for(uint32_t f = 0; f < numFats; f++){
        for(uint32_t s = 1; s < fatSecs; s++){
            if(!sdWriteBlock(fatStart + f * fatSecs + s, b)){
                return 0;
            }
        }
    }
 
    for(int i = 0; i < 512; i++){ b[i] = 0; }
    if(type == 16){
        writeLe16(b, 0, 0xFFF8);
        writeLe16(b, 2, 0xFFFF);
    } else {
        writeLe32(b, 0, 0x0FFFFFF8u);
        writeLe32(b, 4, 0x0FFFFFFFu);
        writeLe32(b, 8, 0x0FFFFFFFu);
    }
    for(uint32_t f = 0; f < numFats; f++){
        if(!sdWriteBlock(fatStart + f * fatSecs, b)){
            return 0;
        }
    }
 
    for(int i = 0; i < 512; i++){ b[i] = 0; }
    if(type == 16){
        for(uint32_t s = 0; s < rootDirSecs; s++){
            if(!sdWriteBlock(rootStart + s, b)){
                return 0;
            }
        }
        if(label && label[0]){
            writeLabelEntry(b, label);
            sdWriteBlock(rootStart, b);
        }
    } else {
        for(uint32_t s = 0; s < spc; s++){
            if(!sdWriteBlock(dataStart + s, b)){
                return 0;
            }
        }
        if(label && label[0]){
            writeLabelEntry(b, label);
            sdWriteBlock(dataStart, b);
        }
    }
 
    return 1;
}
 
void coreStorageCacheVolume(void){
    gVolume.cardType = sdCardGetType();
    gVolume.present = fatGetVolumeInfo(&gVolume.volume);
}

void coreStorageRefresh(void){
    if(fatMount()){
        coreStorageCacheVolume();
    } else {
        gVolume.present = 0;
    }
}

const coreVolume_t *coreStorageVolume(void){
    return &gVolume;
}