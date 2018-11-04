# DripOS
An OS made for fun.
NOTE: This is based off of this tutorial: https://github.com/cfenollosa/os-tutorial


To use:

Linux:

Install qemu for your distro

Run os-image.bin in qemu-system-i386 -fda OR
Run: make run in the DripOS directory

Windows:

Download and install qemu:

https://qemu.weilnetz.de/w64/qemu-w64-setup-20180815.exe

And run os-image.bin:

cd "\Program Files\Qemu"

qemu.exe -m 256 -fda /path/to/Downloads/DripOS-master/DripOS/os-image.bin



Curretly, this OS supports a shell and thats about it.
