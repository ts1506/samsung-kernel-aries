#!/bin/bash

case "$1" in
        galaxys)
            VARIANT="galaxys"
            VER=""
            ;;

        captivate)
            VARIANT="captivate"
            VER="c"
            ;;

        vibrant)
            VARIANT="vibrant"
            VER="v"
            ;;

        *)
            VARIANT="galaxys"
            VER=""
esac

CYAN_VER="CyanCore-v2.8.5"

#export KBUILD_BUILD_VERSION="2"
export LOCALVERSION="-"`echo $CYAN_VER`
#export CROSS_COMPILE=/opt/arm-cortex_a8-linux-gnueabi-linaro_4.7.4-2013.05/bin/arm-cortex_a8-linux-gnueabi-

export CROSS_COMPILE=/opt/arm-cortex_a8-linux-gnueabi-linaro_4.7.4-2013.06/bin/arm-cortex_a8-linux-gnueabi-
export ARCH=arm

echo 
echo "Making ""cyancore"_$VARIANT"_defconfig"

DATE_START=$(date +"%s")

make "cyancore"_$VARIANT"_defconfig"

eval $(grep CONFIG_INITRAMFS_SOURCE .config)
INIT_DIR=$CONFIG_INITRAMFS_SOURCE
MODULES_DIR=`echo $INIT_DIR`files/modules
KERNEL_DIR=`pwd`
OUTPUT_DIR=../output/
CWM_DIR=../ics-ramdisk/cwm/

echo "LOCALVERSION="$LOCALVERSION
echo "CROSS_COMPILE="$CROSS_COMPILE
echo "ARCH="$ARCH
echo "INIT_DIR="$INIT_DIR
echo "MODULES_DIR="$MODULES_DIR
echo "KERNEL_DIR="$KERNEL_DIR
echo "OUTPUT_DIR="$OUTPUT_DIR
echo "CWM_DIR="$CWM_DIR



make -j16 modules

rm `echo $MODULES_DIR"/*"`
find $KERNEL_DIR -name '*.ko' -exec cp -v {} $MODULES_DIR \;
chmod 644 `echo $MODULES_DIR"/*"`

make -j16 zImage

cd arch/arm/boot
tar cvf `echo $CYAN_VER`.tar zImage
mv `echo $CYAN_VER`.tar ../../../$OUTPUT_DIR$VARIANT
echo "Moving to "$OUTPUT_DIR$VARIANT"/"
cd ../../../

cp arch/arm/boot/zImage $CWM_DIR"boot.img"
cd $CWM_DIR
zip -r `echo $CYAN_VER`.zip *
mv  `echo $CYAN_VER`.zip ../$OUTPUT_DIR$VARIANT"/"

DATE_END=$(date +"%s")
echo
DIFF=$(($DATE_END - $DATE_START))
echo "Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
