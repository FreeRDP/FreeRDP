#!/bin/bash -xe
# 
# Copyright 2015 Thincast Technologies GmbH
# 
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script will download and build openssl for iOS and simulator - see ARCHS for architectures built

## Settings
# openssl version to use
OPENSSLVERSION="3.0.2"
SHA256SUM="98e91ccead4d4756ae3c9cde5e09191a8e586d9f4d50838e7ec09d6411dfdb63"
# SDK version to use - if not set latest version found is used
SDK_VERSION=""

# Minimum SDK version the application supports
MIN_SDK_VERSION="15.0"

## Defaults
INSTALLDIR="external"

# Architectures to build
ARCHS="arm64 x86_64"

# Use default SDK version if not set
if [ -z ${SDK_VERSION} ]; then
    SDK_VERSION=`xcrun -sdk iphoneos --show-sdk-version`
fi

CORES=`sysctl hw.ncpu | awk '{print $2}'`
MAKEOPTS="-j $CORES"

DEVELOPER=`xcode-select -print-path`
if [ ! -d "$DEVELOPER" ]; then
  echo "xcode path is not set correctly $DEVELOPER does not exist (most likely because of xcode > 4.3)"
  echo "run"
  echo "sudo xcode-select -switch <xcode path>"
  echo "for default installation:"
  echo "sudo xcode-select -switch /Applications/Xcode.app/Contents/Developer"
  exit 1
fi

# Functions
function buildArch(){
    ARCH=$1
    if [[ "${ARCH}" == "i386" || "${ARCH}" == "x86_64" ]];
    then
        PLATFORM="iPhoneSimulator"
    else
        sed -ie "s!static volatile sig_atomic_t intr_signal;!static volatile intr_signal;!" "crypto/ui/ui_openssl.c"
        PLATFORM="iPhoneOS"
    fi

    export CROSS_TOP="${DEVELOPER}/Platforms/${PLATFORM}.platform/Developer"
    export CROSS_SDK="${PLATFORM}${SDK_VERSION}.sdk"
    export BUILD_TOOLS="${DEVELOPER}"
    export CC="${BUILD_TOOLS}/usr/bin/gcc -arch ${ARCH}"
    if [ ! -z $MIN_SDK_VERSION ]; then
        export CC="$CC -miphoneos-version-min=${MIN_SDK_VERSION}"
    fi
    echo "Building openssl-${OPENSSLVERSION} for ${PLATFORM} ${SDK_VERSION} ${ARCH} (min SDK set: ${MIN_SDK_VERSION:-"none"})"

    LOGFILE="BuildLog.darwin-${ARCH}.txt"
    echo -n " Please wait ..."
    if [[ "$OPENSSLVERSION" =~ 1.0.0. ]]; then
        CONFIG_ARGS=BSD-generic32
    elif [ "${ARCH}" == "x86_64" ]; then
        CONFIG_ARGS=darwin64-x86_64-cc
    elif [ "${ARCH}" == "i386" ]; then
        CONFIG_ARGS="iphoneos-cross no-asm"
    else
        CONFIG_ARGS=iphoneos-cross
    fi

    ./Configure $CONFIG_ARGS  2>&1 | tee ${LOGFILE}

    make ${MAKEOPTS} 2>&1 | tee ${LOGFILE}
    echo " Done. Build log saved in ${LOGFILE}"
    cp libcrypto.a ../../lib/libcrypto_${ARCH}.a
    cp libssl.a ../../lib/libssl_${ARCH}.a
    make clean 2>&1 | tee ${LOGFILE}
}

# main
if [ $# -gt 0 ];then
    INSTALLDIR=$1
    if [ ! -d $INSTALLDIR ];then
        echo "Install directory \"$INSTALLDIR\" does not exist"
        exit 1
    fi
fi

cd $INSTALLDIR
if [ ! -d openssl ];then
    mkdir openssl
fi
cd openssl
CS=`shasum -a 256 "openssl-$OPENSSLVERSION.tar.gz" | cut -d ' ' -f1`
if [ ! "$CS" = "$SHA256SUM" ]; then
    echo "Downloading OpenSSL Version $OPENSSLVERSION ..."
    rm -f "openssl-$OPENSSLVERSION.tar.gz"
    curl -o "openssl-$OPENSSLVERSION.tar.gz" https://www.openssl.org/source/openssl-$OPENSSLVERSION.tar.gz

    CS=`shasum -a 256 "openssl-$OPENSSLVERSION.tar.gz" | cut -d ' ' -f1`
    if [ ! "$CS" = "$SHA256SUM" ]; then
    echo "Download failed or invalid checksum. Have a nice day."
    exit 1
    fi
fi

# remove old build dir
rm -rf openssltmp
mkdir openssltmp
cd openssltmp

echo "Unpacking OpenSSL ..."
tar xfz "../openssl-$OPENSSLVERSION.tar.gz"
if [ ! $? = 0 ]; then
    echo "Unpacking failed."
    exit 1
fi
echo

cd "openssl-$OPENSSLVERSION"

case `pwd` in
     *\ * )
           echo "The build path (`pwd`) contains whitepsaces - fix this."
           exit 1
          ;;
esac

# Cleanup old build artifacts
rm -rf ../../include
mkdir -p ../../include

rm -rf ../../lib
mkdir -p ../../lib

for i in ${ARCHS}; do
    buildArch $i
done

echo "Copying header files ..."
cp -r include/ ../../include/
echo

echo "Combining to unversal binary"
lipo -create ../../lib/libcrypto_*.a -o ../../lib/libcrypto.a
lipo -create ../../lib/libssl_*.a -o ../../lib/libssl.a

echo "Finished. Please verify the contens of the openssl folder in \"$INSTALLDIR\""
