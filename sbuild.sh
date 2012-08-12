#!/bin/bash

sema_ver="Semaphore_JB_2.0.1s"

#export KBUILD_BUILD_VERSION="2"
export LOCALVERSION="-"`echo $sema_ver`

make CROSS_COMPILE=/opt/toolchains/gcc-linaro-arm-linux-gnueabihf-2012.07-20120720_linux/bin/arm-linux-gnueabihf- ARCH=arm -j4 modules

find /kernels/samsung-kernel-aries/ -name '*.ko' -exec cp -v {} /kernels/ics-ramdisk/jb_combo/files/modules \;

make CROSS_COMPILE=/opt/toolchains/gcc-linaro-arm-linux-gnueabihf-2012.07-20120720_linux/bin/arm-linux-gnueabihf- ARCH=arm -j4 zImage

cd arch/arm/boot
tar cvf `echo $sema_ver`.tar zImage
cd ../../../

cp arch/arm/boot/zImage ../ics-ramdisk/cwm/boot.img

cd ../ics-ramdisk/cwm

zip -r `echo $sema_ver`.zip *

mv  `echo $sema_ver`.zip ../


