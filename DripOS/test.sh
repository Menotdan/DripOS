if grub-file --is-x86-multiboot os-image.bin; then
  echo multiboot confirmed
else
  echo the file is not multiboot
fi
