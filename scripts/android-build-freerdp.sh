#!/bin/bash

JPEG_TAG=master
OPENH264_TAG=master
OPENSSL_TAG=master

WITH_JPEG=0
WITH_OPENH264=0
WITH_OPENSSL=0

SRC_DIR=$(pwd)
BUILD_SRC=$(pwd)
BUILD_DST=$(pwd)

CMAKE_BUILD_TYPE=Debug
BUILD_DEPS=0

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
source $SCRIPT_PATH/android-build-common.sh
source $SCRIPT_PATH/android-build.conf

# Parse arguments.
REMAINING=""
while [[ $# > 0 ]]
do
	key="$1"
	case $key in
		--freerdp-src)
			SRC_DIR="$2"
			shift
			;;
		--jpeg)
			WITH_JPEG=1
			shift
			;;
		--openh264)
			WITH_OPENH264=1
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

# clean up top
if [ -d $BUILD_SRC ];
then
	common_clean $BUILD_SRC
fi

if [ -d $BUILD_DST ];
then
	common_run mkdir -p $BUILD_DST
fi

# Prepare the environment
common_run mkdir -p $BUILD_SRC

CMAKE_CMD_ARGS="-DANDROID_NDK=$ANDROID_NDK \
	-DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL} \
	-DCMAKE_TOOLCHAIN_FILE=$SRC_DIR/cmake/AndroidToolchain.cmake \
	-DCMAKE_INSTALL_PREFIX=$BUILD_DST \
	-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
	-DFREERDP_EXTERNAL_PATH=$BUILD_DST"

BASE=$(pwd)
for ARCH in $BUILD_ARCH
do
    # build dependencies.
    if [ $WITH_JPEG -ne 0 ];
    then
        if [ $BUILD_DEPS -ne 0 ];
        then
            common_run bash $SCRIPT_PATH/android-build-jpeg.sh \
                --src $BUILD_SRC/jpeg --dst $BUILD_DST \
                --ndk $ANDROID_NDK \
                --arch $ARCH \
                --tag $JPEG_TAG
        fi
        CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_JPEG=ON"
    fi
    if [ $WITH_OPENH264 -ne 0 ];
    then
        if [ $BUILD_DEPS -ne 0 ];
        then
            common_run bash $SCRIPT_PATH/android-build-openh264.sh \
                --src $BUILD_SRC/openh264 --dst $BUILD_DST \
                --ndk $ANDROID_NDK \
                --arch $ARCH \
                --tag $OPENH264_TAG
        fi
        CMAKE_CMD_ARGS="$CMAKE_CMD_ARGS -DWITH_OPENH264=ON"
    fi
    if [ $WITH_OPENSSL -ne 0 ];
    then
        if [ $BUILD_DEPS -ne 0 ];
        then
            common_run bash $SCRIPT_PATH/android-build-openssl.sh \
                --src $BUILD_SRC/openssl --dst $BUILD_DST \
                --ndk $ANDROID_NDK \
                --arch $ARCH \
                --tag $OPENSSL_TAG
        fi
    fi

    # Build and install the library.
	common_run cd $BASE
	common_run mkdir -p $BUILD_SRC/freerdp-build/$ARCH
	common_run cd $BUILD_SRC/freerdp-build/$ARCH
	common_run export ANDROID_NDK=$ANDROID_NDK
	common_run cmake $CMAKE_CMD_ARGS \
		-DANDROID_ABI=$ARCH \
		$SRC_DIR
	echo $(pwd)
	common_run cmake --build . --target install
done

echo "Successfully build library for architectures $BUILD_ARCH"
