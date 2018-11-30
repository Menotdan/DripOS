# DripOS
An OS made for fun.
NOTE: This is based off of this tutorial: https://github.com/cfenollosa/os-tutorial


### Discord: [Discord](https://discord.gg/E9ZXZWn "Discord")

## Usage:

## Linux:

Install qemu for your distro

Run 
```bash
qemu-system-i386 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -fda os-image.bin
```

## Windows:

Download and install qemu:

https://qemu.weilnetz.de/w64/qemu-w64-setup-20180815.exe

Go to **qemu** directory

```batch
cd "\Program Files\qemu"
```
and to run the emulator with **DripOS**:


```batch
qemu-system-i386.exe -m 256 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -fda /path/to/Downloads/DripOS-master/DripOS/os-image.bin
```
