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

LINKER := linker.ld
ISO := monolithic.iso
PROJECT_ROOT := ../../..
BUILD_DIR    := $(PROJECT_ROOT)/build
KERNEL_BIN   := $(BUILD_DIR)/kernel/monolithic.bin
ISO_STAGING  := $(BUILD_DIR)/iso_staging
DIST_DIR     := $(BUILD_DIR)/dist
ISO_IMAGE    := $(DIST_DIR)/monolithic.iso

.PHONY: all clean

all: $(ISO_IMAGE)

$(ISO_IMAGE): $(KERNEL_BIN) | $(DIST_DIR)
	@mkdir -p $(ISO_STAGING)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_STAGING)/boot/
	@cp boot/grub/grub.cfg $(ISO_STAGING)/boot/grub/
	$(GRUB) -o $@ $(ISO_STAGING)
	@rm -rf $(ISO_STAGING)
	@echo "ISO created: $@"

$(DIST_DIR):
	mkdir -p $@

$(KERNEL_BIN): $(OBJS) $(LINKER) | $(BUILD_DIR)/kernel
	$(CC) -T $(LINKER) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/kernel:
	mkdir -p $@

$(ARCH_DIR)/%.o: $(ARCH_DIR)/%.s
	$(AS) $< -o $@

$(ARCH_DIR)/%.o: $(ARCH_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS)
	rm -rf $(BUILD_DIR)