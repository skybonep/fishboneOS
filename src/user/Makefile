# User-space build system for fishboneOS
# Cross-compiles user programs to ELF binaries for loading by the kernel

CROSS_BIN := $(HOME)/opt/cross/bin
CC := $(CROSS_BIN)/i686-elf-gcc
LD := $(CROSS_BIN)/i686-elf-ld
OBJCOPY := $(CROSS_BIN)/i686-elf-objcopy

# User-space include directories
USER_INC_DIR := include
KERNEL_INC_DIR := ../kernel/monolithic/kernel/include

# Compilation flags for user-space ELF binaries
# -nostdlib: Don't link standard library (we provide our own)
# -static: Static linking for embedded environment
# -Ttext=0x400000: Load user code at 0x00400000 (user code base)
CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
          -I$(USER_INC_DIR) -I$(KERNEL_INC_DIR) \
          -nostdlib -static -Ttext=0x400000

LDFLAGS := -nostdlib -static

# User programs to build
PROGRAMS := init shell hello

# Build directory
BUILD_DIR := build

# Default target
.PHONY: all clean

all: $(addprefix $(BUILD_DIR)/, $(PROGRAMS))

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile init program
$(BUILD_DIR)/init: init.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@.elf $<
	$(OBJCOPY) -O binary $@.elf $@.bin

# Compile shell program
$(BUILD_DIR)/shell: shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@.elf $<
	$(OBJCOPY) -O binary $@.elf $@.bin

# Compile hello program
$(BUILD_DIR)/hello: hello.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@.elf $<
	$(OBJCOPY) -O binary $@.elf $@.bin

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install user programs to disk directory
install: all
	cp $(BUILD_DIR)/*.bin ../../disk/

# Install user programs to disk directory
install: all
	cp $(BUILD_DIR)/*.bin ../../disk/

# Debug target
.PHONY: debug
debug:
	@echo "CROSS_BIN: $(CROSS_BIN)"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "PROGRAMS: $(PROGRAMS)"