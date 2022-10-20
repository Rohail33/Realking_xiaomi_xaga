#!/bin/bash
#set -e
## Copy this script inside the kernel directory
KERNEL_DEFCONFIG=xaga_defconfig
export PATH="/home/rohail33/kernel/clang/bin:$PATH"
export ARCH=arm64
export SUBARCH=arm64
export KBUILD_COMPILER_STRING="$(/home/rohail33/kernel/clang/bin/clang --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')"

if ! [ -d "/home/rohail33/kernel/clang" ]; then
echo "Proton clang not found! Cloning..."
if ! git clone -q https://gitlab.com/ZyCromerZ/clang.git --depth=1 --single-branch /home/rohail/kernel/proton-clang; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

# Speed up build process
MAKE="./makeparallel"
BUILD_START=$(date +"%s")
blue='\033[0;34m'
cyan='\033[0;36m'
yellow='\033[0;33m'
red='\033[0;31m'
nocol='\033[0m'

echo "**** Kernel defconfig is set to $KERNEL_DEFCONFIG ****"
echo -e "$blue***********************************************"
echo "          BUILDING KERNEL          "
echo -e "***********************************************$nocol"
make $KERNEL_DEFCONFIG O=out CC=clang
make -j$(nproc --all) O=out \
                      ARCH=arm64 \
                      CROSS_COMPILE=aarch64-linux-gnu- \
                      CROSS_COMPILE_COMPAT=arm-linux-gnueabi- \
                      LLVM=1 \
                      LLVM_IAS=1
