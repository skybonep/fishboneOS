#
# Makefile for fishboneOS
#
# This file automates the build process:
# - Compiles assembly and C files
# - Links them into an ELF executable
# - Creates a GRUB-bootable ISO image
#
# Usage:
#   make        - Build everything (fishbone.iso)
#   make clean  - Remove all generated files
#   make run    - Run in QEMU (if QEMU is installed)
#

# Cross-compiler toolchain for i386
# These tools compile code to i386 architecture (not your current system)
# CROSS_BIN points to the directory where cross-compiler tools are installed
CROSS_BIN := $(HOME)/opt/cross/bin

CC := $(CROSS_BIN)/i686-elf-gcc          # C compiler
AS := $(CROSS_BIN)/i686-elf-as           # Assembler
LD := $(CROSS_BIN)/i686-elf-ld           # Linker

# Include directories
INCLUDE_DIRS := -Ikernel/include

# Compiler flags
# -std=gnu99: Use C99 standard with GNU extensions
# -ffreestanding: No standard library; we provide our own
# -fno-builtin: Don't use compiler built-in functions
# -Wall -Wextra: Enable all common warnings
# -Wno-unused-parameter: Suppress warnings for unused parameters (common in stubs)
CFLAGS := -std=gnu99 -ffreestanding -fno-builtin -Wall -Wextra -Wno-unused-parameter $(INCLUDE_DIRS)

# Assembler flags (currently empty; could add -g for debug info)
ASFLAGS := 

# Build directories
BUILD_DIR := $(CURDIR)/build
OBJ_DIR := $(BUILD_DIR)/obj
KERNEL_DIR := $(BUILD_DIR)/kernel
ISO_STAGING := $(BUILD_DIR)/iso_staging
DIST_DIR := $(BUILD_DIR)/dist
KERNEL_BIN := $(KERNEL_DIR)/fishbone.elf
ISO_IMAGE := $(DIST_DIR)/fishbone.iso

# Source files - list all .s (assembly) and .c (C) files
SOURCES_S := kernel/src/arch/i386/loader.s kernel/src/arch/i386/io.s
SOURCES_C := kernel/src/kernel/kernel.c kernel/src/drivers/serial.c
OBJECTS := $(patsubst %.s,$(BUILD_DIR)/obj/%.o,$(SOURCES_S)) \
           $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(SOURCES_C))

# Default target - build everything
all: $(ISO_IMAGE)

# Link all .o files into the kernel ELF executable
$(KERNEL_BIN): $(OBJECTS) | $(KERNEL_DIR)
	$(LD) -T linker.ld -o $@ $^

# Create a GRUB-bootable ISO image
# This requires:
# - An ELF kernel ($(KERNEL_BIN))
# - GRUB configuration file (boot/grub/grub.cfg)
# - grub-mkrescue tool (part of GRUB package)
$(ISO_IMAGE): $(KERNEL_BIN) boot/grub/grub.cfg | $(DIST_DIR)
	mkdir -p $(ISO_STAGING)/boot/grub
	cp $(KERNEL_BIN) $(ISO_STAGING)/boot/
	cp boot/grub/grub.cfg $(ISO_STAGING)/boot/grub/
	grub-mkrescue -o $@ $(ISO_STAGING)

$(OBJ_DIR):
	mkdir -p $@

$(KERNEL_DIR):
	mkdir -p $@

$(DIST_DIR):
	mkdir -p $@

# Compile .s (assembly) files to .o (object) files
$(BUILD_DIR)/obj/%.o: %.s | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# Compile .c (C) files to .o (object) files
$(BUILD_DIR)/obj/%.o: %.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean up all generated files
clean:
	rm -rf $(BUILD_DIR)
	rm -rf isodir

# Run the kernel in QEMU
# -cdrom $(ISO_IMAGE): Boot from the ISO image
# -serial stdio: Connect serial port to terminal (for debugging output)
run: $(ISO_IMAGE)
	qemu-system-i386 -cdrom $(ISO_IMAGE) -serial stdio

# Declare non-file targets
.PHONY: all clean run
