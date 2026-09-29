# SyntropyOS
SyntropyOS is a bare-metal operating system for the ESP32 devices with ILI9341 TFT touchscreen displays. Syntropy features a graphical user interface, cooperative multitasking, UART, FrameBuffer, and many features soon to come.

### Supported devices:
Any ESP32 With 2.8 Inch ILI9341 TFT Touch Display Board or larger ILI9341 screens with the XPT2046 resistive touch controller. 

This was developed and tested on 2.8 Inch ESP32 TFT Touch Display Board, those combo boards that include the ESP32, a 2.8 inch TFT resistive touch-screen display, an RGB LED, a microSD card slot and some I/O pins / plugs, but it should compile and work with any ESP32 with an ILI9341 display as long as the pins are correctly set in the OS before compiling.

### Features so far:
- UART for debugging
- FrameBuffer with draw and text primitives so we can easily make UIs
- Stack Protection via hardware watchdogs and kernel panic with proper registers dump and stackshot.
- Cooperative multitasking (threads via round-robin for now)
- GUI
- Touch Screen support with calibration app

### Mind the BARE METAL
Syntropy is a standalone, bare-metal OS, built from scratch, which means it doesn't use any of Espressif's libraries or Stage 2, as such, none of the Espressif APIs you expect are present (no ESP-IDF framework). In other words, this does not run on top of FreeRTOS or Espressif's 2nd Stage Bootloader, so don't expect it to have those same components. 

### Development
- Developed by GeoSn0w (<a href="https://twitter.com/FCE365">Twitter</a>)

For ForenZes Labs
