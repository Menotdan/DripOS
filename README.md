# DripOS
An OS made for fun.
NOTE: This is based off of this tutorial: https://github.com/cfenollosa/os-tutorial


To use:

Linux:

Install qemu for your distro

Run os-image.bin in qemu-system-i386

Windows:

Download and install qemu:

https://qemu.weilnetz.de/w64/qemu-w64-setup-20180815.exe

And run os-image.bin:

cd "\Program Files\Qemu"

qemu.exe -L . -m 256 -soundhw es1370 -net user -net nic,model=rtl8139 -hda /path/to/Downloads/DripOS-master/DripOS/os-image.bin
