#!/bin/bash
SCM_URL=https://github.com/cisco/openh264
SCM_TAG=master

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

function build {
	echo "Building architecture $1..."
	BASE=$(pwd)
	common_run cd $BUILD_SRC
	PATH=$ANDROID_NDK:$PATH
	MAKE="make PATH=$PATH OS=android NDKROOT=$ANDROID_NDK TARGET=android-$2 NDKLEVEL=$2 ARCH=$1 -j libraries"
	common_run git clean -xdf
	common_run $MAKE
	# Install creates a non optimal directory layout, fix that
	common_run $MAKE PREFIX=$BUILD_SRC/libs/$1 install
	common_run cd $BASE
}

# Run the main program.
common_parse_arguments $@
common_check_requirements
common_update $SCM_URL $SCM_TAG $BUILD_SRC
common_clean $BUILD_DST

for ARCH in $BUILD_ARCH
do
	case $ARCH in
	"armeabi")
		OARCH="arm"
	;;
	"armeabi-v7a")
		OARCH="arm"
	;;
	"arm64-v8a")
		OARCH="arm64"
	;;
	*)
		OARCH=$ARCH
	;;
	esac

	echo "$ARCH=$OARCH"

	build $OARCH $NDK_TARGET

	if [ ! -d $BUILD_DST/include ];
	then
		common_run mkdir -p $BUILD_DST/include
	fi

	common_run cp -L -r $BUILD_SRC/libs/$OARCH/include/ $BUILD_DST/
	if [ ! -d $BUILD_DST/$ARCH ];
	then
		common_run mkdir -p $BUILD_DST/$ARCH
	fi
	common_run cp -L $BUILD_SRC/libs/$OARCH/lib/*.so  $BUILD_DST/$ARCH/
done
