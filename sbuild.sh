#!/bin/bash

sema_ver="Semaphore_ICS_1.2.7s"
#sema_ver="Semaphore_JB_0.0.2s"

#export KBUILD_BUILD_VERSION="2"
export LOCALVERSION="-"`echo $sema_ver`

#make CROSS_COMPILE=/opt/toolchains/android-toolchain-eabi-12.01/bin/arm-eabi- ARCH=arm -j4
#make CROSS_COMPILE=/opt/toolchains/arm-2009q3/bin/arm-none-linux-gnueabi- ARCH=arm -j4
#make CROSS_COMPILE=/kernels/semaphore_cap/android/android/system/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi- ARCH=arm -j4 modules
#make CROSS_COMPILE=/opt/toolchains/gcc-linaro-arm-linux-gnueabihf-2012.05-20120523_linux/bin/arm-linux-gnueabihf- ARCH=arm -j4 modules
make CROSS_COMPILE=/tmp/gcc-linaro-arm-linux-gnueabihf-2012.06-20120625_linux/bin/arm-linux-gnueabihf- ARCH=arm -j4 modules

#find /kernels/samsung-kernel-aries/ -name '*.ko' -exec cp -v {} /kernels/ics-ramdisk/cwm/system/lib/modules \;
find /kernels/samsung-kernel-aries/ -name '*.ko' -exec cp -v {} /kernels/ics-ramdisk/ics_combo/files/modules \;

#make CROSS_COMPILE=/kernels/semaphore_cap/android/android/system/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi- ARCH=arm -j4 zImage
make CROSS_COMPILE=/opt/toolchains/gcc-linaro-arm-linux-gnueabihf-2012.06-20120625_linux/bin/arm-linux-gnueabihf- ARCH=arm -j4 zImage

cd arch/arm/boot
tar cvf `echo $sema_ver`.tar zImage
cd ../../../

cp arch/arm/boot/zImage ../ics-ramdisk/cwm/boot.img

cd ../ics-ramdisk/cwm

zip -r `echo $sema_ver`.zip *

mv  `echo $sema_ver`.zip ../


