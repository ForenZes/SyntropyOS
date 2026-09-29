//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
// 

#include "TouchScreen.h"

int TOUCH_MIN_X = 200;
int TOUCH_MAX_X = 3900;
int TOUCH_MIN_Y = 200;
int TOUCH_MAX_Y = 3900;
int TOUCH_SWAP_XY = 1;
int TOUCH_FLIP_X  = 1;
int TOUCH_FLIP_Y  = 0;
int SCREEN_W = 320;
int SCREEN_H = 240;

#define DR_REG_GPIO   0x3FF44000
#define DR_REG_IOMUX  0x3FF49000

#define GPIO_OUT_W1TS           (*(volatile uint32_t*)(DR_REG_GPIO + 0x08))
#define GPIO_OUT_W1TC           (*(volatile uint32_t*)(DR_REG_GPIO + 0x0C))
#define GPIO_ENABLE_W1TS        (*(volatile uint32_t*)(DR_REG_GPIO + 0x24))
#define GPIO_IN                 (*(volatile uint32_t*)(DR_REG_GPIO + 0x3C))
#define GPIO_OUT1_W1TS          (*(volatile uint32_t*)(DR_REG_GPIO + 0x14))
#define GPIO_OUT1_W1TC          (*(volatile uint32_t*)(DR_REG_GPIO + 0x18))
#define GPIO_ENABLE1_W1TS       (*(volatile uint32_t*)(DR_REG_GPIO + 0x2C))
#define GPIO_IN1                (*(volatile uint32_t*)(DR_REG_GPIO + 0x40))
#define GPIO_FUNC_OUT_SEL(n)    (*(volatile uint32_t*)(DR_REG_GPIO + 0x530 + (n)*4))

#define IOMUX_GPIO25 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x24))
#define IOMUX_GPIO32 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x1C))
#define IOMUX_GPIO33 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x20))
#define IOMUX_GPIO36 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x04))
#define IOMUX_GPIO39 (*(volatile uint32_t*)(DR_REG_IOMUX + 0x10))

#define T_CLK  25
#define T_DIN  32
#define T_DOUT 39
#define T_CS   33
#define T_IRQ  36

static void gpoHigh(int p){
    if(p < 32){
        GPIO_OUT_W1TS = (1u << p);
    } else{
        GPIO_OUT1_W1TS = (1u << (p - 32));
    }
}
static void gpoLow(int p){
    if(p < 32){
        GPIO_OUT_W1TC = (1u << p);
    } else{
        GPIO_OUT1_W1TC = (1u << (p - 32));
    }
}
static int gpiRead(int p){
    if(p < 32){
        return (GPIO_IN >> p) & 1;
    }
    return (GPIO_IN1 >> (p - 32)) & 1;
}
static void gpoEnable(int p){
    if(p < 32){
        GPIO_ENABLE_W1TS = (1u << p);
    } else{
        GPIO_ENABLE1_W1TS = (1u << (p - 32));
    }
}
static void tdelay(void){
    for(volatile int i = 0; i < 6; i++){
        __asm__ volatile("nop");
    }
}

void initTouchScreen(void){
    IOMUX_GPIO25 = (2u << 12);
    IOMUX_GPIO32 = (2u << 12);
    IOMUX_GPIO33 = (2u << 12);
    IOMUX_GPIO36 = (1u << 9);
    IOMUX_GPIO39 = (1u << 9);

    GPIO_FUNC_OUT_SEL(T_CLK) = 0x100;
    GPIO_FUNC_OUT_SEL(T_DIN) = 0x100;
    GPIO_FUNC_OUT_SEL(T_CS) = 0x100;
    gpoEnable(T_CLK);
    gpoEnable(T_DIN);
    gpoEnable(T_CS);

    gpoHigh(T_CS);
    gpoLow(T_CLK);
}

static uint16_t xptRead(uint8_t cmd){
    gpoLow(T_CS);
    for(int i = 7; i >= 0; i--){
        if(cmd & (1 << i)){
            gpoHigh(T_DIN);
        }
        else{
            gpoLow(T_DIN);
        }
        tdelay();
        gpoHigh(T_CLK);
        tdelay();
        gpoLow(T_CLK);
    }

    uint16_t v = 0;

    for(int i = 0; i < 16; i++){
        gpoHigh(T_CLK);
        tdelay();
        v = (v << 1) | gpiRead(T_DOUT);
        gpoLow(T_CLK);
        tdelay();
    }

    gpoHigh(T_CS);
    return (v >> 3) & 0x0FFF;
}

int touchScreenGetRaw(uint16_t *rx, uint16_t *ry){
    if(gpiRead(T_IRQ) != 0){
        return 0;
    }

    uint32_t sx = 0, sy = 0;
    const int n = 8;
    xptRead(0xD0);

    for(int i = 0; i < n; i++){
        sx += xptRead(0xD0);
        sy += xptRead(0x90);
    }

    if(gpiRead(T_IRQ) != 0){
        return 0;
    }
    *rx = sx / n;
    *ry = sy / n;
    return 1;
}

static int mapRange(int v, int inLo, int inHi, int outLo, int outHi){
    if(inHi == inLo){
        return outLo;
    }
    
    int r = outLo + (v - inLo) * (outHi - outLo) / (inHi - inLo);

    if(r < outLo){
        r = outLo;
    }

    if(r > outHi){
        r = outHi;
    }
    return r;
}

int touchScreenGet(int *x, int *y){
    uint16_t rx, ry;
    if(!touchScreenGetRaw(&rx, &ry)){
        return 0;
    }

    int sx, sy;

    if(TOUCH_SWAP_XY){
        sx = mapRange(ry, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_W - 1);
        sy = mapRange(rx, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_H - 1);
    } else{
        sx = mapRange(rx, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_W - 1);
        sy = mapRange(ry, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_H - 1);
    }

    if(TOUCH_FLIP_X){
        sx = SCREEN_W - 1 - sx;
    }

    if(TOUCH_FLIP_Y){
        sy = SCREEN_H - 1 - sy;
    }

    *x = sx;
    *y = sy;
    return 1;
}