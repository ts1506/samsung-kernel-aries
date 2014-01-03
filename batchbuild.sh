#!/bin/bash

# Clean working tree
make clean

# Switch to KK 4.4 ramdisk directory
cd ~/semaphore/ics-ramdisk

# Extract tarballs
tar zxvf jb_combo.tar.gz

# Switch to kernel directory
cd ~/semaphore/samsung-kernel-aries

# Build KK 4.4 kernels
sh ./sbuild.sh galaxys
