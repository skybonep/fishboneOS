# Simple Makefile to build barebones.iso from boot.s and kernel.c

CC := i686-elf-gcc
AS := i686-elf-as
GRUB := grub-mkrescue

CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

OBJS := boot.o kernel.o
BIN := myos.bin
ISO := barebones.iso
PROJECT_ROOT := ../../..
ISO_OUT := $(PROJECT_ROOT)/build/iso/boot

.PHONY: all clean

all: $(ISO_OUT)/$(ISO)

$(ISO_OUT)/$(ISO): $(ISO) | $(ISO_OUT)
	cp $< $@

$(ISO): $(BIN)
	$(GRUB) -o $@ .

$(BIN): $(OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

boot.o: boot.s
	$(AS) $< -o $@

kernel.o: kernel.c
	$(CC) -c $(CFLAGS) $< -o $@

$(ISO_OUT):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BIN) $(ISO)