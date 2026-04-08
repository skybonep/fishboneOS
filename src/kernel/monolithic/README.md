# Compile
```
make
make DEBUG=1
```

# Execute
```
qemu-system-i386 -cdrom ../../../build/dist/monolithic.iso -serial stdio
qemu-system-i386 -cdrom ../../../build/dist/monolithic.iso -monitor stdio
qemu-system-i386 -cdrom ../../../build/dist/monolithic.iso -drive file=../../../disk/fat16.img,format=raw,if=ide,index=0 -boot order=d -serial stdio
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

# Create and format disk image
```
dd if=/dev/zero of=disk/fat16.img bs=1M count=32
mkfs.vfat -F 16 disk/fat16.img
```
