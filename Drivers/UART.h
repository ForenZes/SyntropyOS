//
// SyntropyOS
// (C) ForenZes Labs, 2026
// Developed by GeoSn0w (@FCE365)
// https://forenzes.com
//

#ifndef UART_H
#define UART_H

#include <stdint.h>

void uartInit(void);
void uartPutc(char c);
void uartPuts(const char *s);
void uartPrintHex(uint32_t value);
void uartPrintDec(int value);
void uartPrintByteHex(uint8_t byte);
#endif