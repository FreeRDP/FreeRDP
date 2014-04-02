#!/bin/bash
# 
# Copyright 2013 Thincast Technologies GmbH
# 
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script will download and build openssl for iOS (armv7, armv7s) and simulator (i386)

# Settings and definitions
USER_OS_SDK=""
USER_SIM_SDK=""

OPENSSLVERSION="1.0.0e"
MD5SUM="7040b89c4c58c7a1016c0dfa6e821c86"
OPENSSLPATCH="OpenSSL-iFreeRDP.diff"
INSTALLDIR="external"

MAKEOPTS="-j $CORES"
# disable parallell builds since openssl build
# fails sometimes
MAKEOPTS=""
CORES=`sysctl hw.ncpu | awk '{print $2}'`
SCRIPTDIR=$(dirname `cd ${0%/*} && echo $PWD/${0##*/}`)
OS_SDK=""
SIM_SDK=""
OS_SDK_PATH="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs"
SIM_SDK_PATH="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs"

# Functions
function buildArch(){
	ARCH=$1
	LOGFILE="BuildLog.darwin-${ARCH}.txt"
	echo "Building architecture ${ARCH}. Please wait ..."
	./Configure darwin-${ARCH}-cc > ${LOGFILE}
	make ${MAKEOPTS} >> ${LOGFILE} 2>&1
	echo "Done. Build log saved in ${LOGFILE}"
	cp libcrypto.a ../../lib/libcrypto_${ARCH}.a
	cp libssl.a ../../lib/libssl_${ARCH}.a
	make clean >/dev/null 2>&1
	echo
}

# main
if [ $# -gt 0 ];then
	INSTALLDIR=$1
	if [ ! -d $INSTALLDIR ];then
		echo "Install directory \"$INSTALLDIR\" does not exist"
		exit 1
	fi
fi

echo "Detecting SDKs..."
if [ "x${USER_OS_SDK}" == "x" ];then
	OS_SDK=`ls -1 ${OS_SDK_PATH} | sort -n | head -1`
	if [ "x${OS_SDK}" == "x" ];then
		echo "No iPhoneOS SDK found"
		exit 1;
	fi
else
	OS_SDK=${USER_OS_SDK}
	if [ ! -d "${OS_SDK_PATH}/${OS_SDK}" ];then
		echo "User specified iPhoneOS SDK not found"
		exit 1
	fi
fi
echo "Using iPhoneOS SDK: ${OS_SDK}"

if [ "x${USER_SIM_SDK}" == "x" ];then
	SIM_SDK=`ls -1 ${SIM_SDK_PATH} | sort -n | head -1`
	if [ "x${SIM_SDK}" == "x" ];then
		echo "No iPhoneSimulator SDK found"
		exit 1;
	fi
else
	SIM_SDK=${USER_SIM_SDK}
	if [ ! -d "${SIM_SDK_PATH}/${SIM_SDK}" ];then
		echo "User specified iPhoneSimulator SDK not found"
		exit 1
	fi
fi
echo "Using iPhoneSimulator SDK: ${SIM_SDK}"
echo

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

echo "Applying iFreeRDP patch ..."
cd "openssl-$OPENSSLVERSION"
cp ${SCRIPTDIR}/${OPENSSLPATCH} .
sed -ie "s#__ISIMSDK__#${SIM_SDK}#" ${OPENSSLPATCH}
sed -ie "s#__IOSSDK__#${OS_SDK}#" ${OPENSSLPATCH}

patch -p1 < $OPENSSLPATCH

if [ ! $? = 0 ]; then
    echo "Patch failed."
    exit 1
fi
echo

# Cleanup old build artifacts
mkdir -p ../../include/openssl
rm -f ../../include/openssl/*.h

mkdir -p ../../lib
rm -f ../../lib/*.a

echo "Copying header hiles ..."
cp include/openssl/*.h ../../include/openssl/
echo

buildArch i386
buildArch armv7
buildArch armv7s

echo "Combining to unversal binary"
lipo -create ../../lib/libcrypto_*.a -o ../../lib/libcrypto.a
lipo -create ../../lib/libssl_*.a -o ../../lib/libssl.a

echo "Finished. Please verify the contens of the openssl folder in \"$INSTALLDIR\""
