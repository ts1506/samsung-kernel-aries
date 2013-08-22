#!/bin/bash

# Clean working tree
make clean

# Switch to JB 4.2 ramdisk directory
cd ~/semaphore/ics-ramdisk-4.2

# Extract tarballs
tar zxvf jb_combo.tar.gz
tar zxvf jb_combo_c.tar.gz
tar zxvf jb_combo_v.tar.gz

# Switch to kernel directory
cd ~/semaphore/samsung-kernel-aries

# Build JB 4.2 kernels
sh ./sbuild.sh galaxys42
sh ./sbuild.sh captivate42
sh ./sbuild.sh vibrant42

# Clean working tree
make clean

# Switch to JB 4.3 ramdisk directory
cd ~/semaphore/ics-ramdisk

# Extract tarballs
tar zxvf jb_combo.tar.gz

# Switch to kernel directory
cd ~/semaphore/samsung-kernel-aries

# Build JB 4.2 kernels
sh ./sbuild.sh galaxys
