#!/bin/bash

SCM_URL=https://www.openssl.org/source
SCM_TAG=master

COMPILER=4.9

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

function build {
	if [ $# -ne 2 ];
	then
		echo "Invalid arguments $@"
		exit 1
	fi

	CONFIG=$1
    DST_PREFIX=$2

    common_run export CC=clang
    common_run export PATH=$(${SCRIPT_PATH}/toolchains_path.py --ndk ${ANDROID_NDK}):$ORG_PATH
    common_run export ANDROID_NDK

	echo "CONFIG=$CONFIG"
	echo "DST_PREFIX=$DST_PREFIX"
	echo "PATH=$PATH"

	BASE=$(pwd)
	DST_DIR=$BUILD_DST/$DST_PREFIX
	common_run cd $BUILD_SRC
	common_run ./Configure ${CONFIG} -D__ANDROID_API__=$NDK_TARGET
	common_run make SHLIB_EXT=.so -j build_libs

	if [ ! -d $DST_DIR ];
	then
		common_run mkdir -p $DST_DIR
	fi

    common_run cp *.so $DST_DIR/
	common_run cd $BASE
}

# Run the main program.
common_parse_arguments $@
common_check_requirements
common_update $SCM_URL $SCM_TAG $BUILD_SRC

ORG_PATH=$PATH
for ARCH in $BUILD_ARCH
do

	case $ARCH in
	 "armeabi-v7a")
		 build "android-arm" "armeabi-v7a"
         ;;
	 "x86")
		 build "android-x86" "x86"
		 ;;
	 "arm64-v8a")
		 build "android-arm64" "arm64-v8a"
		 ;;
	 "x86_64")
		 build "android-x86_64" "x86_64"
		 ;;
	*)
		echo "[WARNING] Skipping unsupported architecture $ARCH"
		continue
		;;
	esac
done

if [ ! -d $BUILD_DST/$ARCH/include ];
then
	common_run mkdir -p $BUILD_DST/$ARCH/include
fi
common_run cp -L -R $BUILD_SRC/include/openssl $BUILD_DST/$ARCH/include/

