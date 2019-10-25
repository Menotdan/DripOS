# DripOS
[![][discord_image]][discord_link]

An OS made for fun.

NOTE: This is based off of this tutorial: https://github.com/cfenollosa/os-tutorial


### Discord: [Discord](https://discord.gg/E9ZXZWn "Discord")

## Usage:

## Booting on real hardware:
Note: I am not responsible for any damage to your system, although I don't expect any, as I test DripOS on real hardware quite a bit
### Step One:
Write DripOS.iso to a USB drive. On Linux, this is ```sudo dd if=DripOS.iso of=/dev/yourdrivehere```
Make sure to select the correct drive, otherwise you may overwrite your main boot drive with DripOS code.
### Step Two:
Reboot your computer and enter the BIOS, then select the USB drive as the boot device.
You can enter the BIOS on many systems by pressing DEL a bunch of times during startup.

## Linux:

Install qemu for your distro

Run 
```bash
qemu-system-i386 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -fda os-image.bin
```

## Windows:

Download and install qemu (pick 32 or 64 bit version):

https://qemu.weilnetz.de/

Go to **qemu** directory

```batch
cd "\Program Files\qemu"
```
and to run the emulator with **DripOS**:


```batch
qemu-system-i386.exe -m 256 -soundhw pcspk -device isa-debug-exit,iobase=0xf4,iosize=0x04 -fda /path/to/Downloads/DripOS-master/DripOS/os-image.bin
```
[discord_image]:https://img.shields.io/badge/discord-DripOS-738bd7.svg?style=square
[discord_link]:https://discord.gg/E9ZXZWn
