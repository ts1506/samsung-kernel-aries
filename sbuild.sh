#!/bin/bash

sema_ver="Semaphore_ICS_1.0.0sbm"

#export KBUILD_BUILD_VERSION="2"
export LOCALVERSION="-"`echo $sema_ver`

#make CROSS_COMPILE=/opt/toolchains/android-toolchain-eabi-12.01/bin/arm-eabi- ARCH=arm -j4
#make CROSS_COMPILE=/opt/toolchains/arm-2009q3/bin/arm-none-linux-gnueabi- ARCH=arm -j4
make CROSS_COMPILE=/kernels/semaphore_cap/android/android/system/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi- ARCH=arm -j4

find /kernels/samsung-kernel-aries/ -name '*.ko' -exec cp -v {} /kernels/ics-ramdisk/cwm/system/lib/modules \;

cd ../ics-ramdisk/
./build_boot.sh

cp boot.img ./cwm

cd cwm

zip -r `echo $sema_ver`.zip *

mv  `echo $sema_ver`.zip ../


