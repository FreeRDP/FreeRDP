#!/bin/bash -e
#
# Copyright 2015 Thincast Technologies GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Download and build static OpenSSL libraries for iOS, producing two installs:
#
#   build/ios.openssl.build       arm64 iOS device            (-DPLATFORM=OS64)
#   build/ios-sim.openssl.build   arm64 + x86_64 iOS Simulator (-DPLATFORM=SIMULATORARM64
#                                                                or SIMULATOR64)
#
# A device and a simulator slice of the SAME architecture (arm64) carry different
# Mach-O platform markers and cannot live in one fat library, so the simulator
# gets its own install directory. There the arm64 and x86_64 simulator slices
# (same platform, different arch) are combined with lipo, so the simulator build
# runs natively on Apple Silicon and on Intel Macs.

## Settings
# openssl version to use
OPENSSLVERSION="3.4.0"
SHA256SUM="e15dda82fe2fe8139dc2ac21a36d4ca01d5313c75f99f46c4e8a27709b7294bf"
# Minimum SDK version the application supports
MIN_SDK_VERSION="15.0"

## Defaults
# Output goes under build/ which is already git-ignored.
INSTALLDIR="build"

DEVELOPER=`xcode-select -print-path`
if [ ! -d "$DEVELOPER" ]; then
  echo "xcode path is not set correctly $DEVELOPER does not exist"
  echo "run"
  echo "sudo xcode-select -switch <xcode path>"
  echo "for default installation:"
  echo "sudo xcode-select -switch /Applications/Xcode.app/Contents/Developer"
  exit 1
fi

CORES=`sysctl hw.ncpu | awk '{print $2}'`
MAKEOPTS="-j $CORES"

# main
if [ $# -gt 0 ];then
    INSTALLDIR=$1
    if [ ! -d $INSTALLDIR ];then
        echo "Install directory \"$INSTALLDIR\" does not exist"
        exit 1
    fi
fi

# Absolute paths so the per-slice builds (which cd into temporary trees) resolve.
mkdir -p "$INSTALLDIR"
INSTALLDIR=`cd "$INSTALLDIR" && pwd`
# Source cache lives outside the install dirs so wiping an install never destroys
# the downloaded tarball.
CACHE="$INSTALLDIR/.openssl-src"
TARBALL="$CACHE/openssl-$OPENSSLVERSION.tar.gz"

mkdir -p "$CACHE"
CS=`shasum -a 256 "$TARBALL" 2>/dev/null | cut -d ' ' -f1`
if [ ! "$CS" = "$SHA256SUM" ]; then
    echo "Downloading OpenSSL Version $OPENSSLVERSION ..."
    rm -f "$TARBALL"
    curl -Lo "$TARBALL" https://github.com/openssl/openssl/releases/download/openssl-$OPENSSLVERSION/openssl-$OPENSSLVERSION.tar.gz
    CS=`shasum -a 256 "$TARBALL" | cut -d ' ' -f1`
    if [ ! "$CS" = "$SHA256SUM" ]; then
        echo "Download failed or invalid checksum. Have a nice day."
        exit 1
    fi
fi

# buildSlice <platform> <sdk-name> <arch> <config-extra> <cc-extra>
# Builds one OpenSSL slice in a fresh source tree and echoes the build directory
# (containing libcrypto.a, libssl.a and the generated include/ tree).
buildSlice() {
    local PLATFORM="$1" SDKNAME="$2" ARCH="$3" CONFIG_EXTRA="$4" CC_EXTRA="$5"
    local SDK_VERSION WORK
    SDK_VERSION=`xcrun -sdk "$SDKNAME" --show-sdk-version`
    WORK="$CACHE/build-$PLATFORM-$ARCH"

    echo "Building openssl-${OPENSSLVERSION} for ${PLATFORM} ${SDK_VERSION} ${ARCH}" 1>&2
    rm -rf "$WORK"; mkdir -p "$WORK"
    tar xfz "$TARBALL" -C "$WORK"
    cd "$WORK/openssl-$OPENSSLVERSION"

    case `pwd` in *\ *) echo "The build path (`pwd`) contains whitespaces - fix this." 1>&2; exit 1 ;; esac

    if [ "$PLATFORM" == "iPhoneOS" ]; then
        sed -ie "s!static volatile intr_signal;!static volatile sig_atomic_t intr_signal;!" "crypto/ui/ui_openssl.c"
    fi

    export CROSS_TOP="${DEVELOPER}/Platforms/${PLATFORM}.platform/Developer"
    export CROSS_SDK="${PLATFORM}${SDK_VERSION}.sdk"
    export CC="${DEVELOPER}/usr/bin/gcc -arch ${ARCH} ${CC_EXTRA}"

    ./Configure iphoneos-cross ${CONFIG_EXTRA} >"BuildLog-${PLATFORM}-${ARCH}.txt" 2>&1
    make ${MAKEOPTS} >>"BuildLog-${PLATFORM}-${ARCH}.txt" 2>&1
    # OpenSSL 3.x generates the public headers (ssl.h, crypto.h, ...) from .h.in
    # templates at build time - make sure they exist before they get copied.
    make build_generated >>"BuildLog-${PLATFORM}-${ARCH}.txt" 2>&1
    echo "$WORK/openssl-$OPENSSLVERSION"
}

# installDevice <build-dir>
installDevice() {
    local SRC="$1" DST="$INSTALLDIR/ios.openssl.build"
    rm -rf "$DST"; mkdir -p "$DST/lib"
    cp "$SRC/libcrypto.a" "$DST/lib/libcrypto.a"
    cp "$SRC/libssl.a"    "$DST/lib/libssl.a"
    cp -r "$SRC/include/" "$DST/include/"
    echo "  -> device install in $DST"
}

# installSimulator <arm64-build-dir> <x86_64-build-dir>
installSimulator() {
    local A="$1" X="$2" DST="$INSTALLDIR/ios-sim.openssl.build"
    rm -rf "$DST"; mkdir -p "$DST/lib"
    lipo -create "$A/libcrypto.a" "$X/libcrypto.a" -o "$DST/lib/libcrypto.a"
    lipo -create "$A/libssl.a"    "$X/libssl.a"    -o "$DST/lib/libssl.a"
    cp -r "$A/include/" "$DST/include/"
    echo "  -> simulator (arm64+x86_64) install in $DST"
}

DEV_ARM64=`buildSlice iPhoneOS iphoneos arm64 "" "-miphoneos-version-min=${MIN_SDK_VERSION}"`
installDevice "$DEV_ARM64"

SIM_ARM64=`buildSlice iPhoneSimulator iphonesimulator arm64 "no-asm" \
    "-target arm64-apple-ios${MIN_SDK_VERSION}-simulator -mios-simulator-version-min=${MIN_SDK_VERSION}"`
SIM_X86_64=`buildSlice iPhoneSimulator iphonesimulator x86_64 "no-asm" \
    "-target x86_64-apple-ios${MIN_SDK_VERSION}-simulator -mios-simulator-version-min=${MIN_SDK_VERSION}"`
installSimulator "$SIM_ARM64" "$SIM_X86_64"

cd "$INSTALLDIR"
rm -rf "$CACHE"

echo ""
echo "Finished."
echo "  device    : build/ios.openssl.build       (-DPLATFORM=OS64)"
echo "  simulator : build/ios-sim.openssl.build    (-DPLATFORM=SIMULATORARM64 or SIMULATOR64,"
echo "                                              -DFREERDP_IOS_EXTERNAL_SSL_PATH=build/ios-sim.openssl.build)"
