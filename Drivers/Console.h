#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void syConsoleInit(uint16_t foregroundColor, uint16_t backgroundColor, int scale);
void syConsolePutChar(char character);
void syConsolePutString(const char *string);

#endif
