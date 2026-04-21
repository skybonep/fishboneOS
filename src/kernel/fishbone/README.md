# fishboneOS Kernel

This is the fishbone kernel implementation - a minimal, educational x86 operating system kernel.

## Compile

```bash
# Build release version (default)
make

# Build with debug symbols and test harness
make debug

# Build release version explicitly
make release
```

## Test

```bash
# Run automated tests (builds with DEBUG=1, runs QEMU, captures serial output)
make test

# Run tests and show full serial log output
make test-verbose
```

## Execute

```bash
# Run in QEMU with serial output to terminal
make run

# Manual QEMU commands
qemu-system-i386 -cdrom build/dist/fishbone.iso -serial stdio
qemu-system-i386 -cdrom build/dist/fishbone.iso -display none -serial file:serial.log
timeout 10s qemu-system-i386 -cdrom build/dist/fishbone.iso -display none -serial file:serial.log -no-reboot
```

## Debug

```bash
# QEMU monitor commands (Ctrl-Alt-2 to switch to monitor)
(qemu) info registers    # Show CPU registers
(qemu) info mem          # Show memory info
(qemu) info tlb          # Show TLB info
(qemu) quit              # Exit QEMU

# Switch back to normal view
Ctrl-Alt-1
```

## Clean

```bash
# Remove all build artifacts
make clean
```

## Architecture

- **CPU**: i386 (32-bit x86)
- **Bootloader**: GRUB multiboot
- **Memory**: Flat 4GB address space
- **Output**: Serial console (COM1)
- **Build**: GCC cross-compiler toolchain
