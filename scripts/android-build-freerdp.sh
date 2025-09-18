#!/bin/bash

OPENH264_TAG=v2.6.0
OPENH264_HASH=558544ad358283a7ab2930d69a9ceddf913f4a51ee9bf1bfb9e377322af81a69
OPENSSL_TAG=openssl-3.5.3
OPENSSL_HASH=c9489d2abcf943cdc8329a57092331c598a402938054dc3a22218aea8a8ec3bf
FFMPEG_TAG=n7.1.2
FFMPEG_HASH=7ddad2d992bd250a6c56053c26029f7e728bebf0f37f80cf3f8a0e6ec706431a
CJSON_TAG=v1.7.19
CJSON_HASH=7fa616e3046edfa7a28a32d5f9eacfd23f92900fe1f8ccd988c1662f30454562

WITH_OPENH264=0
WITH_OPENSSL=0
WITH_FFMPEG=0
WITH_AAD=0

SRC_DIR=$(dirname "${BASH_SOURCE[0]}")
SRC_DIR=$(realpath "$SRC_DIR")
BUILD_SRC=$(pwd)
BUILD_DST=$(pwd)

CMAKE_BUILD_TYPE=Debug
BUILD_DEPS=0

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
source $SCRIPT_PATH/android-build-common.sh
source $SCRIPT_PATH/android-build.conf

# Parse arguments.
REMAINING=""
while [[ $# > 0 ]]; do
  key="$1"
  case $key in
  --freerdp-src)
    SRC_DIR="$2"
    shift
    ;;
  --openh264)
    WITH_OPENH264=1
    shift
    ;;
  --openh264-ndk)
    shift
    ANDROID_NDK_OPENH264=$1
    shift
    ;;
  --ffmpeg)
    WITH_FFMPEG=1
    shift
    ;;
  --cjson)
    WITH_AAD=1
    shift
    ;;
  --openssl)
    WITH_OPENSSL=1
    shift
    ;;
  --debug)
    CMAKE_BUILD_TYPE=Debug
    shift
    ;;
  --release)
    CMAKE_BUILD_TYPE=Release
    shift
    ;;
  --relWithDebug)
    CMAKE_BUILD_TYPE=RelWithDebug
    shift
    ;;
  --build-deps)
    BUILD_DEPS=1
    shift
    ;;
  *)
    REMAINING="$REMAINING $key"
    shift
    ;;
  esac
done
common_parse_arguments $REMAINING

if [ -z ${WITH_MEDIACODEC+x} ]; then
  common_run echo "WITH_MEDIACODEC unset, defining WITH_MEDIACODEC=1"
  WITH_MEDIACODEC=1
fi

# clean up top
if [ -d $BUILD_SRC ]; then
  common_clean $BUILD_SRC
fi

if [ -d $BUILD_DST ]; then
  common_run mkdir -p $BUILD_DST
fi

# Prepare the environment
common_run mkdir -p $BUILD_SRC

CMAKE_CMD_ARGS="-DANDROID_NDK=$ANDROID_NDK \
	-DANDROID_NATIVE_API_LEVEL=android-${NDK_TARGET} \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
	-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
	-DFREERDP_EXTERNAL_PATH=$BUILD_DST \
	-DWITHOUT_FREERDP_3x_DEPRECATED=ON \
	-DWITH_CLIENT_SDL=OFF \
	-DWITH_SERVER=OFF \
	-DWITH_INTERNAL_RC4=ON \
	-DWITH_INTERNAL_MD4=ON \
	-DWITH_INTERNAL_MD5=ON \
	-DWITH_MANPAGES=OFF \
	-DCMAKE_MAKE_PROGRAM=make"

BASE=$(pwd)
for ARCH in $BUILD_ARCH; do
  # build dependencies.
  if [ $WITH_OPENH264 -ne 0 ]; then
    if [ -z "$ANDROID_NDK_OPENH264" ]; then
      echo
      echo "Warning: Missing openh264-ndk, using $ANDROID_NDK" >&2
      echo
      ANDROID_NDK_OPENH264=$ANDROID_NDK
    fi
    if [ $BUILD_DEPS -ne 0 ]; then
      common_run bash $SCRIPT_PATH/android-build-openh264.sh \
        --src $BUILD_SRC/openh264 --dst $BUILD_DST \
        --sdk "$ANDROID_SDK" \
        --ndk "$ANDROID_NDK_OPENH264" \
        --arch $ARCH \
        --target $NDK_TARGET \
        --tag $OPENH264_TAG \
        --hash $OPENH264_HASH
    fi
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_OPENH264=ON"
  else
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_OPENH264=OFF"
  fi

  if [ $WITH_MEDIACODEC -ne 0 ]; then
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_MEDIACODEC=ON"
  else
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_MEDIACODEC=OFF"
  fi

  if [ $WITH_FFMPEG -ne 0 ]; then
    if [ $BUILD_DEPS -ne 0 ]; then
      common_run bash $SCRIPT_PATH/android-build-ffmpeg.sh \
        --src $BUILD_SRC/ffmpeg --dst $BUILD_DST \
        --sdk "$ANDROID_SDK" \
        --ndk "$ANDROID_NDK" \
        --arch $ARCH \
        --target $NDK_TARGET \
        --tag $FFMPEG_TAG \
        --hash $FFMPEG_HASH
    fi
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_FFMPEG=ON -DWITH_SWCALE=ON"
  else
    CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_FFMPEG=OFF -DWITH_SWSCALE=OFF"
  fi
  if [ $WITH_AAD -ne 0 ]; then
    if [ $BUILD_DEPS -ne 0 ]; then
      common_run bash $SCRIPT_PATH/android-build-cjson.sh \
        --src $BUILD_SRC/cjson --dst $BUILD_DST \
        --sdk "$ANDROID_SDK" \
        --ndk "$ANDROID_NDK" \
        --arch $ARCH \
        --target $NDK_TARGET \
        --tag $CJSON_TAG \
        --hash $CJSON_HASH
    fi
  fi
  if [ $WITH_OPENSSL -ne 0 ]; then
    if [ $BUILD_DEPS -ne 0 ]; then
      common_run bash $SCRIPT_PATH/android-build-openssl.sh \
        --src $BUILD_SRC/openssl --dst $BUILD_DST \
        --sdk "$ANDROID_SDK" \
        --ndk $ANDROID_NDK \
        --arch $ARCH \
        --target $NDK_TARGET \
        --tag $OPENSSL_TAG \
        --hash $OPENSSL_HASH
    fi
  fi

  # Build and install the library.
  if [ $DEPS_ONLY -eq 0 ]; then
    common_run cd $BASE
    common_run mkdir -p $BUILD_SRC/freerdp-build/$ARCH
    common_run cd $BUILD_SRC/freerdp-build/$ARCH
    common_run export ANDROID_NDK=$ANDROID_NDK
    common_run $CMAKE_PROGRAM $CMAKE_CMD_ARGS \
      -DANDROID_ABI=$ARCH \
      -DCMAKE_INSTALL_PREFIX=$BUILD_DST/$ARCH \
      -DCMAKE_INSTALL_LIBDIR=. \
      -DCMAKE_PREFIX_PATH=$BUILD_DST/$ARCH \
      -DCMAKE_SHARED_LINKER_FLAGS="-L$BUILD_DST/$ARCH" \
      -DcJSON_DIR=$BUILD_DST/$ARCH/cmake/cJSON \
      $SRC_DIR
    echo $(pwd)
    common_run $CMAKE_PROGRAM --build . --target install
  fi
done

echo "Successfully build library for architectures $BUILD_ARCH"
