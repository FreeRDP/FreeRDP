#!/bin/sh
# 
# Copyright 2013 Thinstuff Technologies GmbH
# 
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script will download and build openssl for iOS (armv7, armv7s) and i386
#

OPENSSLVERSION="1.0.0e"
MD5SUM="7040b89c4c58c7a1016c0dfa6e821c86"
OPENSSLPATCH="../../scripts/OpenSSL-iFreeRDP.diff"
CORES=`sysctl hw.ncpu | awk '{print $2}'`

MAKEOPTS="-j $CORES"
# disable parallell builds since openssl build
# fails sometimes
MAKEOPTS=""

cd external
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
patch -p1 < "../../$OPENSSLPATCH"

if [ ! $? = 0 ]; then
    echo "Patch failed."
    exit 1
fi
echo

mkdir -p ../../include/openssl
rm -f ../../include/openssl/*.h

mkdir -p ../../lib
rm -f ../../lib/*.a

echo "Copying header hiles ..."
cp include/openssl/*.h ../../include/openssl/
echo

echo "Building sim version (for simulator). Please wait ..."
./Configure darwin-sim-cc >BuildLog.darwin-sim.txt
make ${MAKEOPTS} >>BuildLog.darwin-sim.txt 2>&1
echo "Done. Build log saved in BuildLog.darwin-sim.txt"
cp libcrypto.a ../../lib/libcrypto_sim.a
cp libssl.a ../../lib/libssl_sim.a
make clean >/dev/null 2>&1
echo

echo "Building armv7 version (for iPhone). Please wait ..."
./Configure darwin-armv7-cc >BuildLog.darwin-armv7.txt
make ${MAKEOPTS} >>BuildLog.darwin-armv7.txt 2>&1
echo "Done. Build log saved in BuildLog.darwin-armv7.txt"
cp libcrypto.a ../../lib/libcrypto_armv7.a
cp libssl.a ../../lib/libssl_armv7.a
make clean >/dev/null 2>&1
echo

echo "Building armv7s version (for iPhone). Please wait ..."
./Configure darwin-armv7s-cc >BuildLog.darwin-armv7s.txt
make ${MAKEOPTS} >>BuildLog.darwin-armv7s.txt 2>&1
echo "Done. Build log saved in BuildLog.darwin-armv7s.txt"
cp libcrypto.a ../../lib/libcrypto_armv7s.a
cp libssl.a ../../lib/libssl_armv7s.a
make clean >/dev/null 2>&1
echo

echo "Combining to unversal binary"
lipo -create ../../lib/libcrypto_sim.a ../../lib/libcrypto_armv7.a ../../lib/libcrypto_armv7s.a -o ../../lib/libcrypto.a
lipo -create ../../lib/libssl_sim.a ../../lib/libssl_armv7.a ../../lib/libssl_armv7s.a -o ../../lib/libssl.a

echo "Finished. Please verify the contens of the openssl folder in your main project folder"

