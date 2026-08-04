#!/bin/bash

# TO RUN THIS SCRIPT: chmod +x board.sh
# THEN RUN: sudo true (this is because sudo asks for password once and then authenticates you for about 5 minutes)
# NOW, WE CAN EXECUTE: ./board.sh

lsblk #check that the usb is mounted, i should see sda1 or smth like that here potentially have an if/else that greps and checks it
sudo mkdir -p /lib/firmware/xilinx/finger #replace with your project name if you're changing it (we're not)
# sudo mount /dev/sda1 /mnt #optional really
# sudo cp /mnt/dft.bin /mnt/pl.dtbo /mnt/shell.json /lib/firmware/xilinx/dft/ #if using mount

# this script expects that you've put your files naked (no subfolders) on the toshiba thingy
# if you have your files in a folder, change the cp source to '/run/media/TSB USB DRV-sda1/FOLDER_NAME/finger.bin'
sudo cp '/run/media/TSB USB DRV-sda1/finger.bin' /lib/firmware/xilinx/finger/ || echo "copying finger.bin failed"
sudo cp '/run/media/TSB USB DRV-sda1/pl.dtbo' /lib/firmware/xilinx/finger/ || echo "copying pl.dtbo failed"
sudo cp '/run/media/TSB USB DRV-sda1/shell.json' /lib/firmware/xilinx/finger/ || echo "copying shell.json failed"

sudo cp '/run/media/TSB USB DRV-sda1/finger_host' /home/petalinux/ || echo "copying finger_host failed"
sudo cp '/run/media/TSB USB DRV-sda1/finger.bin' /home/petalinux/ || echo "copying finger_bin to /home/petalinux failed"
cd /home/petalinux/
sudo chmod 644 finger.bin
sudo chmod +x finger_host

# List current apps to see if anything is loaded
sudo xmutil listapps

# Unload any existing application (like the vadd one)
sudo xmutil unloadapp

# Load fingerprint
sudo xmutil loadapp finger

./finger_host -x finger.bin
# unmount cmd: sudo umount '/run/media/TSB USB DRV-sda1'
