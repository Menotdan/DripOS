# DripOS
[![][discord_image]][discord_link]

Yet another hobby x86_64 OS.

## Build instructions:
# Requirements:
x86_64-elf-gcc (cross compiler) >= 9.2.0

xorriso >= 1.4.8

grub-mkrescue >= 2.04

make
# Building:
Go to the directory where you downloaded the source, and then type `make`. This should generate a file called `DripOS.iso`

# Running:
You can run this in qemu with the recommended command `sudo qemu-system-x86_64 -machine q35 -enable-kvm -smp 2 -cpu host -m 2G -cdrom DripOS.iso`. You can also change -smp 2 to -smp <number of cores you have> for more cores.

## NOTE: I am not responsible for any damage to any system you choose to run this on, but this should be mostly safe
To run this on real hardware, take a USB drive and write `DripOS.iso` to the drive. (NOT as a file, using some image burning tool like `dd` on linux) Then you can plug it into a computer and boot from it.


[discord_image]:https://img.shields.io/badge/discord-DripOS-738bd7.svg?style=square
[discord_link]:https://discord.gg/E9ZXZWn
