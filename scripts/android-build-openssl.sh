#!/bin/bash

SCM_URL=https://github.com/openssl/openssl
SCM_TAG=master

COMPILER=4.9

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

function build {
	if [ $# -ne 5 ];
	then
		echo "Invalid arguments $@"
		exit 1
	fi

	CONFIG=$1
	ARCH_PREFIX=$2
	DST_PREFIX=$3
	TOOLCHAIN_PREFIX=$4
	PLATFORM_PREFIX=$5

	TMP_DIR=$ANDROID_NDK/toolchains/$TOOLCHAIN_PREFIX$COMPILER/prebuilt/
	HOST_PLATFORM=$(ls $TMP_DIR)
	if [ ! -d $TMP_DIR$HOST_POLATFORM ];
	then
		echo "could not determine NDK host platform in $ANDROID_NDK/toolchains/$TOOLCHAIN_PREFIX$COMPILER/prebuilt/"
		exit 1
	fi

	common_run export CROSS_SYSROOT=$ANDROID_NDK/platforms/android-$NDK_TARGET/$PLATFORM_PREFIX
	common_run export ANDROID_DEV=$ANDROID_NDK/platforms/android-$NDK_TARGET/$PLATFORM_PREFIX/usr
	common_run export CROSS_COMPILE=$ARCH_PREFIX
	common_run export PATH=$ANDROID_NDK/toolchains/$TOOLCHAIN_PREFIX$COMPILER/prebuilt/$HOST_PLATFORM/bin/:$ORG_PATH

	echo "CONFIG=$CONFIG"
	echo "ARCH_PREFIX=$ARCH_PREFIX"
	echo "DST_PREFIX=$DST_PREFIX"
	echo "TOOLCHAIN_PREFIX=$TOOLCHAIN_PREFIX"
	echo "PLATFORM_PREFIX=$PLATFORM_PREFIX"
	echo "CROSS_SYSROOT=$CROSS_SYSROOT"
	echo "CROSS_COMPILE=$CROSS_COMPILE"
	echo "PATH=$PATH"

	BASE=$(pwd)
	DST_DIR=$BUILD_DST/$DST_PREFIX
	common_run cd $BUILD_SRC
	common_run git am $(dirname "${BASH_SOURCE[0]}")/0001-64bit-architecture-support.patch
	common_run git clean -xdf
	common_run ./Configure --openssldir=$DST_DIR $CONFIG shared
	common_run make CALC_VERSIONS="SHLIB_COMPAT=; SHLIB_SOVER=" depend
	common_run make CALC_VERSIONS="SHLIB_COMPAT=; SHLIB_SOVER=" build_libs

	if [ ! -d $DST_DIR ];
	then
		common_run mkdir -p $DST_DIR
	fi

	common_run cp -L libssl.so $DST_DIR
	common_run cp -L libcrypto.so $DST_DIR
	common_run cd $BASE
}

# Run the main program.
common_parse_arguments $@
common_check_requirements
common_update $SCM_URL $SCM_TAG $BUILD_SRC
common_clean $BUILD_DST

ORG_PATH=$PATH
for ARCH in $BUILD_ARCH
do

	case $ARCH in
	"armeabi")
		 build "android" "arm-linux-androideabi-" \
			$ARCH "arm-linux-androideabi-" "arch-arm"
		 ;;
	 "armeabi-v7a")
		 build "android-armv7" "arm-linux-androideabi-" \
			$ARCH "arm-linux-androideabi-" "arch-arm"
		 ;;
	 "mips")
		 build "android-mips" "mipsel-linux-android-" \
			$ARCH "mipsel-linux-android-" "arch-mips"
		 ;;
	 "mips64")
		 build "android64-mips64" "mips64el-linux-android-" \
			$ARCH "mips64el-linux-android-" "arch-mips64"
		 ;;
	 "x86")
		 build "android-x86" "i686-linux-android-" \
			$ARCH "x86-" "arch-x86"
		 ;;
	 "arm64-v8a")
		 build "android64-aarch64" "aarch64-linux-android-" \
			$ARCH "aarch64-linux-android-" "arch-arm64"
		 ;;
	 "x86_64")
		 build "android64" "x86_64-linux-android-" \
			$ARCH "x86_64-" "arch-x86_64"
		 ;;
	*)
		echo "[WARNING] Skipping unsupported architecture $ARCH"
		continue
		;;
	esac
done

if [ ! -d $BUILD_DST/include ];
then
	common_run mkdir -p $BUILD_DST/include
fi
common_run cp -L -r $BUILD_SRC/include/openssl $BUILD_DST/include/

