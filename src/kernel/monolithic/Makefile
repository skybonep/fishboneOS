CROSS_BIN := $(HOME)/opt/cross/bin
KERNEL_DIR := kernel/src/kernel/kernel
ARCH_DIR := kernel/src/kernel/arch/i386
LIBC_DIR := libc
MODULES_DIR := modules

DEBUG ?= 0

KERNEL_INC_DIR := kernel/include
LIBC_INC_DIR := libc/include

CC := $(CROSS_BIN)/i686-elf-gcc
AS := $(CROSS_BIN)/i686-elf-as
OBJCOPY := $(CROSS_BIN)/i686-elf-objcopy
GRUB := grub-mkrescue

CPPFLAGS := -I$(KERNEL_INC_DIR) -I$(LIBC_INC_DIR) -D__is_libk
ifneq ($(DEBUG),0)
CPPFLAGS += -DDEBUG
endif

CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra $(CPPFLAGS)
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

# auto-discover kernel C sources and assembly sources
SOURCES := $(shell find kernel libc -type f \( -name '*.c' -o -name '*.s' \) 2>/dev/null)
# Exclude user_init.c since we now use menu_user_task.c
SOURCES := $(filter-out kernel/src/kernel/user_init.c, $(SOURCES))

OBJS := $(patsubst %.c,%.o,$(SOURCES))
OBJS := $(patsubst %.s,%.o,$(OBJS))

PROJECT_ROOT := $(abspath ../../..)
BUILD_DIR    := $(PROJECT_ROOT)/build

# Module build targets
MODULE_SOURCES := $(MODULES_DIR)/hello_module.c
MODULE_OBJS := $(patsubst %.c,%.o,$(MODULE_SOURCES))
EMBEDDED_SRC := $(BUILD_DIR)/modules/embedded_modules.c
EMBEDDED_OBJ := $(BUILD_DIR)/modules/embedded_modules.o

LINKER := linker.ld
ISO := monolithic.iso
KERNEL_BIN   := $(BUILD_DIR)/kernel/monolithic.bin
ISO_STAGING  := $(BUILD_DIR)/iso_staging
DIST_DIR     := $(BUILD_DIR)/dist
ISO_IMAGE    := $(DIST_DIR)/monolithic.iso

.PHONY: all clean debug release

debug: DEBUG=1
debug: all

release: DEBUG=0
release: all

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

$(KERNEL_BIN): $(OBJS) $(EMBEDDED_OBJ) $(LINKER) | $(BUILD_DIR)/kernel
	$(CC) -T $(LINKER) -o $@ $(OBJS) $(EMBEDDED_OBJ) $(LDFLAGS)

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

$(MODULES_DIR)/%.o: $(MODULES_DIR)/%.c
	$(CC) -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(KERNEL_INC_DIR) -I$(LIBC_INC_DIR) $< -o $@

$(EMBEDDED_SRC): $(MODULE_OBJS)
	mkdir -p $(BUILD_DIR)/modules
	printf '/* Auto-generated embedded module data */\n' > $@
	printf '#include <stdint.h>\n\n' >> $@
	sh -c 'xxd -i "$$1" | sed -e "1s/^unsigned char .*$$/const uint8_t embedded_hello_module[] = {/" -e "s/^unsigned int .*_len = .*$$/const uint32_t embedded_hello_module_size = sizeof(embedded_hello_module);/" >> "$$2"' _ "$<" "$@"

$(EMBEDDED_OBJ): $(EMBEDDED_SRC)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS)
	rm -f $(MODULE_OBJS)
	rm -f $(MODULE_BIN)
	rm -f $(EMBEDDED_OBJ)
	rm -rf $(BUILD_DIR)