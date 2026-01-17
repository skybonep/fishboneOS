CROSS_BIN := $(HOME)/opt/cross/bin
KERNEL_DIR := kernel/src/kernel/kernel
ARCH_DIR := kernel/src/kernel/arch/i386
LIBC_DIR := libc

KERNEL_INC_DIR := kernel/include
LIBC_INC_DIR := libc/include

CC := $(CROSS_BIN)/i686-elf-gcc
AS := $(CROSS_BIN)/i686-elf-as
GRUB := grub-mkrescue

CPPFLAGS := -I$(KERNEL_INC_DIR) -I$(LIBC_INC_DIR) -D__is_libk
CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra $(CPPFLAGS)
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

# auto-discover kernel C sources and assembly sources
SOURCES := $(shell find kernel libc -type f \( -name '*.c' -o -name '*.s' \) 2>/dev/null)

OBJS := $(patsubst %.c,%.o,$(SOURCES))
OBJS := $(patsubst %.s,%.o,$(OBJS))

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

$(ARCH_DIR)/%.o: $(ARCH_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(ISO_OUT):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BIN)
	rm -f $(ISO_OUT)/$(ISO)