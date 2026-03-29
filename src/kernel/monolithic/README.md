# Compile
```
make
```

# Execute
```
qemu-system-i386 -cdrom ../../../build/dist/monolithic.iso -serial stdio
qemu-system-i386 -cdrom ../../../build/dist/monolithic.iso -monitor stdio
```

# Debug
```
Ctrl-Alt-1: normal view
Ctrl-Alt-2: monitor view
(qemu) info registers
(qemu) info mem
(qemu) info tlb
(qemu) quit
```

# Clean
```
make clean
```
