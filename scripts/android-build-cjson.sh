#!/bin/bash

SCM_URL=https://github.com/DaveGamble/cJSON/archive
SCM_TAG=v1.7.15
SCM_HASH=5308fd4bd90cef7aa060558514de6a1a4a0819974a26e6ed13973c5f624c24b2

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

# Run the main program.
common_parse_arguments $@
common_check_requirements
common_update $SCM_URL $SCM_TAG $BUILD_SRC $SCM_HASH

# Prepare the environment
common_run mkdir -p $BUILD_SRC

CMAKE_CMD_ARGS="-DANDROID_NDK=$ANDROID_NDK \
	-DANDROID_NATIVE_API_LEVEL=android-${NDK_TARGET} \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
	-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
	-DENABLE_CJSON_TEST=OFF \
	-DENABLE_HIDDEN_SYMBOLS=OFF \
	-DCMAKE_MAKE_PROGRAM=make"

BASE=$(pwd)
for ARCH in $BUILD_ARCH
do
	common_run cd $BASE
	common_run mkdir -p $BUILD_SRC/cJSON-build/$ARCH
	common_run cd $BUILD_SRC/cJSON-build/$ARCH
	common_run export ANDROID_NDK=$ANDROID_NDK
	common_run $CMAKE_PROGRAM $CMAKE_CMD_ARGS \
		-DANDROID_ABI=$ARCH \
		-DCMAKE_INSTALL_PREFIX=$BUILD_DST/$ARCH \
		-DCMAKE_INSTALL_LIBDIR=. \
		-B . \
		-S $BUILD_SRC
	echo $(pwd)
	common_run $CMAKE_PROGRAM --build . --target install
done

echo "Successfully build library for architectures $BUILD_ARCH"
