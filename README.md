# DripOS
[![][discord_image]][discord_link]

Yet another hobby x86_64 OS.

## Build instructions:
# Requirements:
x86_64-elf-gcc (cross compiler) >= 9.2.0

i386/i686-elf-gcc (cross compiler for building qloader2) >= 9.2.0

[echfs-utils](https://github.com/qword-os/echfs) (For building the image)

parted

make
# Building:
(Remeber to first edit [this line](https://github.com/Menotdan/DripOS/blob/7e88a8ca17d21889f12074f4bef5a57cca4d8f24/Makefile#L73) and set the CC variable to the cross compiler that you downloaded for i386-elf-gcc or i686-elf-gcc)


Go to the directory where you downloaded the source, and then type `make update_qloader2`, and then `make`. This should generate a file called `DripOS.img`

# Running:
You can run this in qemu (x86_64 variant) with the recommended command `sudo qemu-system-x86_64 -machine q35 -enable-kvm -smp 2 -cpu host -m 2G -hda DripOS.img`. You can also change -smp 2 to -smp <number of cores you have> for more cores.

## NOTE: I am not responsible for any damage to any system you choose to run this on, but this should be mostly safe
To run this on real hardware, take a USB drive and write `DripOS.img` to the drive. (NOT as a file, using some image burning tool like `dd` on linux) Then you can plug it into a computer and boot from it.

[a screenshot of dripos][dripos_screenshot1.png]

[discord_image]:https://img.shields.io/badge/discord-DripOS-738bd7.svg?style=square
[discord_link]:https://discord.gg/E9ZXZWn
