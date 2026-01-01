# Simple Makefile to build monolithic.iso from boot.s and kernel.c

CROSS_BIN := $(HOME)/opt/cross/bin
ARCH_DIR := kernel/arch/i386
CC := $(CROSS_BIN)/i686-elf-gcc
AS := $(CROSS_BIN)/i686-elf-as
GRUB := grub-mkrescue

CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

OBJS := $(ARCH_DIR)/boot.o kernel.o
BIN := monolithic.bin
LINKER := linker.ld
ISO := monolithic.iso
PROJECT_ROOT := ../../..
ISO_OUT := $(PROJECT_ROOT)/build/iso/boot

.PHONY: all clean

all: $(ISO_OUT)/$(ISO)

$(ISO_OUT)/$(ISO): $(BIN) | $(ISO_OUT)
	$(GRUB) -o $@ .
	@echo "ISO created: $@"

$(BIN): $(OBJS) $(LINKER)
	$(CC) -T $(LINKER) -o $@ $(OBJS) $(LDFLAGS)

$(ARCH_DIR)/%.o: $(ARCH_DIR)/%.s
	$(AS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(ISO_OUT):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BIN)
	rm -f $(ISO_OUT)/$(ISO)