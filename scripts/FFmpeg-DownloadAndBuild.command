#!/bin/bash -e
#
# Copyright 2025 Thincast Technologies GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Download and build FFmpeg for iOS as a dynamic FFmpeg.framework, mirroring
# scripts/OpenSSL-DownloadAndBuild.command. It produces two installs:
#
#   build/ios.ffmpeg.build/FFmpeg.framework       arm64 iOS device            (-DPLATFORM=OS64)
#   build/ios-sim.ffmpeg.build/FFmpeg.framework   arm64 + x86_64 iOS Simulator (-DPLATFORM=SIMULATORARM64
#                                                                                or SIMULATOR64)
#
# Like OpenSSL, a device and a simulator slice of the same architecture (arm64)
# cannot share one binary, so the simulator framework is a separate install whose
# arm64 and x86_64 simulator slices are merged with lipo.
#
# Only libavcodec + libavutil are built (LGPL, no GPL) with the H.264/HEVC
# decoders and VideoToolbox hardware acceleration - this is all FreeRDP needs for
# the /gfx:AVC420 (H.264) GFX pipeline. The static libraries are merged into one
# dynamic framework binary (install_name @rpath/FFmpeg.framework/FFmpeg) so the
# LGPL relinking requirement stays satisfiable for App Store distribution.
#
# Use it when running cmake (FindFFmpeg is bypassed by pointing it at the
# framework directly), e.g. for the simulator:
#   -DWITH_FFMPEG=ON -DWITH_VIDEO_FFMPEG=ON -DWITH_DSP_FFMPEG=OFF
#   -DWITH_SWSCALE=OFF -DWITH_VIDEOTOOLBOX=ON
#   -DAVCODEC_INCLUDE_DIRS=build/ios-sim.ffmpeg.build/FFmpeg.framework/Headers
#   -DAVUTIL_INCLUDE_DIRS=build/ios-sim.ffmpeg.build/FFmpeg.framework/Headers
#   -DAVCODEC_LIBRARIES=build/ios-sim.ffmpeg.build/FFmpeg.framework/FFmpeg
#   -DAVUTIL_LIBRARIES=build/ios-sim.ffmpeg.build/FFmpeg.framework/FFmpeg

## Settings
# FFmpeg version to use
FFMPEGVERSION="7.1.1"
SHA256SUM="733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1"
# Minimum SDK version the application supports
MIN_SDK_VERSION="15.0"

## Defaults
# Output goes under build/ which is already git-ignored.
INSTALLDIR="build"

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode not configured. Run: sudo xcode-select -switch /Applications/Xcode.app/Contents/Developer"
    exit 1
fi
CORES=`sysctl hw.ncpu | awk '{print $2}'`

# main
if [ $# -gt 0 ];then
    INSTALLDIR=$1
    if [ ! -d $INSTALLDIR ];then
        echo "Install directory \"$INSTALLDIR\" does not exist"
        exit 1
    fi
fi

mkdir -p "$INSTALLDIR"
INSTALLDIR=`cd "$INSTALLDIR" && pwd`
CACHE="$INSTALLDIR/.ffmpeg-src"
TARBALL="$CACHE/ffmpeg-$FFMPEGVERSION.tar.xz"

mkdir -p "$CACHE"
CS=`shasum -a 256 "$TARBALL" 2>/dev/null | cut -d ' ' -f1`
if [ ! "$CS" = "$SHA256SUM" ]; then
    echo "Downloading FFmpeg Version $FFMPEGVERSION ..."
    rm -f "$TARBALL"
    curl -Lo "$TARBALL" https://ffmpeg.org/releases/ffmpeg-$FFMPEGVERSION.tar.xz
    CS=`shasum -a 256 "$TARBALL" | cut -d ' ' -f1`
    if [ ! "$CS" = "$SHA256SUM" ]; then
        echo "Download failed or invalid checksum. Have a nice day."
        exit 1
    fi
fi

# buildSlice <sdk-name> <arch> <min-flag> <triple-or-empty>
# Builds one slice and echoes the path to its per-arch dynamic FFmpeg binary.
buildSlice() {
    local SDKNAME="$1" ARCH="$2" MINFLAG="$3" TRIPLE="$4"
    local SDK WORK STAGE
    SDK=`xcrun --sdk "$SDKNAME" --show-sdk-path`
    WORK="$CACHE/build-$SDKNAME-$ARCH"
    STAGE="$WORK/stage"

    echo "Building FFmpeg ${FFMPEGVERSION} for ${SDKNAME} ${ARCH}" 1>&2
    rm -rf "$WORK"; mkdir -p "$WORK"
    tar xf "$TARBALL" -C "$WORK"
    cd "$WORK/ffmpeg-$FFMPEGVERSION"

    local FLAGS="-arch ${ARCH} ${MINFLAG} -isysroot ${SDK} ${TRIPLE}"

    ./configure \
        --prefix="$STAGE" \
        --enable-cross-compile --target-os=darwin --arch=${ARCH} --cc=clang \
        --sysroot="$SDK" \
        --extra-cflags="$FLAGS" \
        --extra-ldflags="$FLAGS" \
        --disable-gpl --enable-static --disable-shared --enable-pic \
        --disable-x86asm \
        --disable-programs --disable-doc --disable-debug --disable-network --disable-autodetect \
        --disable-everything \
        --disable-avdevice --disable-avformat --disable-avfilter \
        --disable-swscale --disable-swresample --disable-postproc \
        --enable-avcodec --enable-avutil \
        --enable-decoder=h264 --enable-parser=h264 \
        --enable-decoder=hevc --enable-parser=hevc \
        --enable-videotoolbox \
        --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox \
        >"$WORK/configure.log" 2>&1
    make -j"${CORES}" >"$WORK/build.log" 2>&1
    make install >>"$WORK/build.log" 2>&1

    # Merge the static libs into one per-arch dynamic binary. libavcodec has
    # circular intra-archive references (e.g. h264_sei <-> h2645_sei) that the
    # Apple linker only resolves with -force_load.
    clang -dynamiclib -arch "${ARCH}" -isysroot "${SDK}" ${MINFLAG} ${TRIPLE} \
        -install_name @rpath/FFmpeg.framework/FFmpeg \
        -compatibility_version ${FFMPEGVERSION} -current_version ${FFMPEGVERSION} \
        -Wl,-force_load,"$STAGE/lib/libavcodec.a" \
        -Wl,-force_load,"$STAGE/lib/libavutil.a" \
        -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework CoreFoundation \
        -o "$WORK/FFmpeg-${ARCH}"
    echo "$WORK"
}

# assembleFramework <install-dir> <supported-platform> <headers-stage> <binary> [<binary2>]
assembleFramework() {
    local DST="$1" PLATSUPPORT="$2" HSTAGE="$3"; shift 3
    local FW="$DST/FFmpeg.framework"
    rm -rf "$DST"; mkdir -p "$FW/Headers" "$FW/Modules"

    lipo -create "$@" -o "$FW/FFmpeg"
    cp -R "$HSTAGE/include/libavcodec" "$HSTAGE/include/libavutil" "$FW/Headers/"

    cat > "$FW/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleExecutable</key><string>FFmpeg</string>
  <key>CFBundleIdentifier</key><string>org.ffmpeg.FFmpeg</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleName</key><string>FFmpeg</string>
  <key>CFBundlePackageType</key><string>FMWK</string>
  <key>CFBundleShortVersionString</key><string>${FFMPEGVERSION}</string>
  <key>CFBundleVersion</key><string>${FFMPEGVERSION}</string>
  <key>MinimumOSVersion</key><string>${MIN_SDK_VERSION}</string>
  <key>CFBundleSupportedPlatforms</key><array>${PLATSUPPORT}</array>
</dict></plist>
PLIST

    cat > "$FW/Modules/module.modulemap" <<'MOD'
framework module FFmpeg {
  umbrella "Headers"
  export *
  module * { export * }
}
MOD
    echo "  -> $FW"
}

# arm64 device
DEV=`buildSlice iphoneos arm64 "-miphoneos-version-min=${MIN_SDK_VERSION}" ""`
assembleFramework "$INSTALLDIR/ios.ffmpeg.build" "<string>iPhoneOS</string>" "$DEV/stage" "$DEV/FFmpeg-arm64"

# arm64 + x86_64 simulator
SIM_ARM=`buildSlice iphonesimulator arm64 "-mios-simulator-version-min=${MIN_SDK_VERSION}" \
    "-target arm64-apple-ios${MIN_SDK_VERSION}-simulator"`
SIM_X86=`buildSlice iphonesimulator x86_64 "-mios-simulator-version-min=${MIN_SDK_VERSION}" \
    "-target x86_64-apple-ios${MIN_SDK_VERSION}-simulator"`
assembleFramework "$INSTALLDIR/ios-sim.ffmpeg.build" "<string>iPhoneSimulator</string>" "$SIM_ARM/stage" \
    "$SIM_ARM/FFmpeg-arm64" "$SIM_X86/FFmpeg-x86_64"

cd "$INSTALLDIR"
rm -rf "$CACHE"

echo ""
echo "Finished. Dynamic FFmpeg.framework built for both platforms:"
echo "  device    : build/ios.ffmpeg.build/FFmpeg.framework        (-DPLATFORM=OS64)"
echo "  simulator : build/ios-sim.ffmpeg.build/FFmpeg.framework     (-DPLATFORM=SIMULATORARM64 or SIMULATOR64)"
lipo -info "$INSTALLDIR/ios.ffmpeg.build/FFmpeg.framework/FFmpeg"
lipo -info "$INSTALLDIR/ios-sim.ffmpeg.build/FFmpeg.framework/FFmpeg"
