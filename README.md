# DripOS
Support: [Discord Server](https://discord.gg/E9ZXZWn)  
  
An OS made for fun.  
  
NOTE: This project is based on tutorials from [os-dev](https://github.com/cfenollosa/os-tutorial)  
NOTE: There are preview builds on the support server  
  
## Usage
### Linux

Install qemu for your distro  
  
Run os-image.bin by
```
qemu-system-i386 -fda <path to os-image.bin>
```
or by using the provided makefile script
```
make run
```

### Windows

Download and install qemu from [here](https://qemu.weilnetz.de/w64/qemu-w64-setup-20180815.exe)

And run `os-image.bin` by typing these commands into the command line

```
cd "\Program Files\Qemu"
qemu.exe -m 256 -fda /path/to/Downloads/DripOS-master/DripOS/os-image.bin
```


Currently, this OS supports a shell and thats about it.
