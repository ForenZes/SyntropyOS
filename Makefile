
TOOLCHAIN ?= /Volumes/DATA/SyntropyOS/xtensa-esp-elf/bin
VENV ?= /Volumes/DATA/SyntropyOS/venv/bin
CC = $(TOOLCHAIN)/xtensa-esp32-elf-gcc
ESPTOOL = $(VENV)/esptool.py
PORT = /dev/cu.usbserial-160
 
CSRC = $(shell find . -name '*.c')
ASRC = $(shell find . -name '*.S')
OBJS = $(ASRC:.S=.o) $(CSRC:.c=.o)
INCDIRS = $(sort $(dir $(shell find . -name '*.h'))) ./
CFLAGS = -std=c11 -Os -ffreestanding -nostdlib -mlongcalls -mabi=call0 $(addprefix -I,$(INCDIRS))
 
.PHONY: all flash clean
 
all: syntropy.bin
 
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
 
%.o: %.S
	$(CC) -mabi=call0 -mlongcalls -c $< -o $@
 
app.elf: $(OBJS) esp32.ld
	$(CC) -nostdlib -mabi=call0 -T esp32.ld $(OBJS) -o app.elf -lgcc
 
syntropy.bin: app.elf
	$(ESPTOOL) --chip esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB -o syntropy.bin app.elf
 
flash: syntropy.bin
	$(ESPTOOL) --chip esp32 --port $(PORT) write_flash 0x1000 syntropy.bin
 
clean:
	find . -name '*.o' -delete
	rm -f app.elf syntropy.bin