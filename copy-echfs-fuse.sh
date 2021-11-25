#!/bin/bash

mkdir mountpoint
echfs-fuse temp-image mountpoint/
cp -R /var/dripos-sysroot/* mountpoint/
fusermount -u mountpoint
