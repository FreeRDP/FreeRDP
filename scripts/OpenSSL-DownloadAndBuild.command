#!/bin/bash
# 
# Copyright 2015 Thincast Technologies GmbH
# 
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script will download and build openssl for iOS and simulator - see ARCHS for architectures built

## Settings
# openssl version to use
OPENSSLVERSION="1.0.2f"
MD5SUM="b3bf73f507172be9292ea2a8c28b659d"
# SDK version to use - if not set latest version found is used
SDK_VERSION=""

# Minimum SDK version the application supports
MIN_SDK_VERSION=""


## Defaults
INSTALLDIR="external"

# Architectures to build
ARCHS="i386 x86_64 armv7 armv7s arm64"

# Use default SDK version if not set
if [ -z ${SDK_VERSION} ]; then
	SDK_VERSION=`xcrun -sdk iphoneos --show-sdk-version`
fi

CORES=`sysctl hw.ncpu | awk '{print $2}'`
MAKEOPTS="-j $CORES"
# disable parallell builds since openssl build
# fails sometimes
MAKEOPTS=""

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
		./Configure BSD-generic32 > "${LOGFILE}" 2>&1
	elif [ "${ARCH}" == "x86_64" ]; then
		./Configure darwin64-x86_64-cc > "${LOGFILE}" 2>&1
	elif [ "${ARCH}" == "i386" ]; then
		./Configure iphoneos-cross no-asm > "${LOGFILE}" 2>&1
	else
		./Configure iphoneos-cross  > "${LOGFILE}" 2>&1
	fi

	make ${MAKEOPTS} >> ${LOGFILE} 2>&1
	echo " Done. Build log saved in ${LOGFILE}"
	cp libcrypto.a ../../lib/libcrypto_${ARCH}.a
	cp libssl.a ../../lib/libssl_${ARCH}.a
	make clean >/dev/null 2>&1
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
CS=`md5 -q "openssl-$OPENSSLVERSION.tar.gz" 2>/dev/null`
if [ ! "$CS" = "$MD5SUM" ]; then
    echo "Downloading OpenSSL Version $OPENSSLVERSION ..."
    rm -f "openssl-$OPENSSLVERSION.tar.gz"
    curl -o "openssl-$OPENSSLVERSION.tar.gz" http://www.openssl.org/source/openssl-$OPENSSLVERSION.tar.gz

    CS=`md5 -q "openssl-$OPENSSLVERSION.tar.gz" 2>/dev/null`
    if [ ! "$CS" = "$MD5SUM" ]; then
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
mkdir -p ../../include/openssl
rm -f ../../include/openssl/*.h

mkdir -p ../../lib
rm -f ../../lib/*.a

echo "Copying header files ..."
cp include/openssl/*.h ../../include/openssl/
echo

for i in ${ARCHS}; do
	buildArch $i
done

echo "Combining to unversal binary"
lipo -create ../../lib/libcrypto_*.a -o ../../lib/libcrypto.a
lipo -create ../../lib/libssl_*.a -o ../../lib/libssl.a

echo "Finished. Please verify the contens of the openssl folder in \"$INSTALLDIR\""
