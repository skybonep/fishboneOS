# Simple Makefile to build barebones.iso from boot.s and kernel.c

CROSS_BIN := $(HOME)/opt/cross/bin
CC := $(CROSS_BIN)/i686-elf-gcc
AS := $(CROSS_BIN)/i686-elf-as
GRUB := grub-mkrescue

CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

OBJS := boot.o kernel.o
BIN := barebones.bin
LINKER := linker.ld
ISO := barebones.iso
PROJECT_ROOT := ../../..
ISO_OUT := $(PROJECT_ROOT)/build/iso/boot

.PHONY: all clean

all: $(ISO_OUT)/$(ISO)

$(ISO_OUT)/$(ISO): $(BIN) | $(ISO_OUT)
	$(GRUB) -o $@ .
	@echo "ISO created: $@"

$(BIN): $(OBJS) $(LINKER)
	$(CC) -T $(LINKER) -o $@ $(OBJS) $(LDFLAGS)

boot.o: boot.s
	$(AS) $< -o $@

kernel.o: kernel.c
	$(CC) -c $(CFLAGS) $< -o $@

$(ISO_OUT):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BIN)
	rm -f $(ISO_OUT)/$(ISO)