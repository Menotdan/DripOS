#!/bin/bash

shopt -s dotglob
shopt -s globstar

OLD_WD=$(pwd)
IMAGE_PATH=$(realpath $2)

cd $1

ROOT_FILES=$(echo **)
for i in $ROOT_FILES; do
  echo "Copying file $i to image..."
  echfs-utils "$IMAGE_PATH" import "$i" "$i" &> /dev/null
done

cd $OLD_WD
