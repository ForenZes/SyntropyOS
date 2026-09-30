//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#include "UART.h"

#define DR_REG_UART0 0x3FF40000
#define UART0_FIFO   (*(volatile uint32_t*)(DR_REG_UART0 + 0x00))
#define UART0_STATUS (*(volatile uint32_t*)(DR_REG_UART0 + 0x1C))
#define UART0_CLKDIV (*(volatile uint32_t*)(DR_REG_UART0 + 0x14))
#define UART0_CONF0  (*(volatile uint32_t*)(DR_REG_UART0 + 0x20))

void uartInit(void){
    UART0_CONF0 = (3u << 2) | (1u << 4) | (1u << 27);
    UART0_CLKDIV = 347u | (3u << 20);
}

void uartPutc(char c){
    while(((UART0_STATUS >> 16) & 0xFF) >= 120){
    }
    UART0_FIFO = (uint32_t)(uint8_t)c;
}

void uartPuts(const char *s){
    while(*s){
        if(*s == '\n'){
            uartPutc('\r');
        }
        uartPutc(*s);
        s++;
    }
}

void uartPrintHex(uint32_t value){
    static const char hexDigits[] = "0123456789ABCDEF";
    uartPutc('0');
    uartPutc('x');
    for(int i = 0; i < 8; i++){
        uartPutc(hexDigits[(value >> ((7 - i) * 4)) & 0xF]);
    }
}

void uartPrintByteHex(uint8_t byte){
    const char *hexDigits = "0123456789ABCDEF";
    uartPutc(hexDigits[(byte >> 4) & 0xF]);
    uartPutc(hexDigits[byte & 0xF]);
}

void uartPrintDec(int value){
    char rev[12];
    int n = 0;
    unsigned u = (unsigned)value;

    if(value < 0){
        uartPutc('-');
        u = (unsigned)(-value);
    }
    if(u == 0){
        uartPutc('0');
        return;
    }
    while(u){
        rev[n++] = '0' + (u % 10);
        u /= 10;
    }
    while(n){
        uartPutc(rev[--n]);
    }
}