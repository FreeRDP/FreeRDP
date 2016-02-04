#!/bin/bash

if [ -z $BUILD_ARCH ]; then
	BUILD_ARCH="armeabi armeabi-v7a mips mips64 x86 x86_64 arm64-v8a"
fi

if [ -z $NDK_TARGET ]; then
	NDK_TARGET=21
fi

if [ -z $ANDROID_NDK ]; then
	ANDROID_NDK="missing"
fi

if [ -z $BUILD_DST ]; then
	BUILD_DST=$(pwd)/libs
fi

if [ -z $BUILD_SRC ]; then
	BUILD_SRC=$(pwd)/src
fi

if [ -z $SCM_URL ]; then
	SCM_URL="missing"
fi

if [ -z $SCM_TAG ]; then
	SCM_TAG=master
fi

CLEAN_BUILD_DIR=0

function common_help {
	echo "$(BASHSOURCE[0]) supports the following arguments:"
	echo "	--ndk	The base directory of your android NDK defa"
	echo "			ANDROID_NDK=$ANDROID_NDK"
	echo "	--arch	A list of architectures to build"
	echo "			BUILD_ARCH=$BUILD_ARCH"
	echo "	--dst	The destination directory for include and library files"
	echo "			BUILD_DST=$BUILD_DST"
	echo "	--src	The source directory for SCM checkout"
	echo "			BUILD_SRC=$BUILD_SRC"
	echo "	--url	The SCM source url"
	echo "			SCM_URL=$SCM_URL"
	echo "	--tag	The SCM branch or tag to check out"
	echo "			SCM_TAG=$SCM_TAG"
	echo "	--clean	Clean the destination before build"
	echo "	--help	Display this help"
	exit 0
}

function common_run {
	echo "[RUN] $@"
	"$@"
	RES=$?
	if [[ $RES -ne 0 ]];
	then
		echo "[ERROR] $@ retured $RES"
		exit 1
	fi
}

function common_parse_arguments {
	while [[ $# > 0 ]]
	do
		key="$1"
		case $key in
		    --conf)
            source "$2"
            shift
            ;;

			--ndk)
			ANDROID_NDK="$2"
			shift
			;;

			--arch)
			BUILD_ARCH="$2"
			shift
			;;

			--dst)
			BUILD_DST="$2"
			shift
			;;

			--src)
			BUILD_SRC="$2"
			shift
			;;

			--url)
			SCM_URL="$2"
			shift
			;;

			--tag)
			SCM_TAG="$2"
			shift
			;;

			--clean)
			CLEAN_BUILD_DIR=1
			shift
			;;

			--help)
			common_help
			shift
			;;

			*) # Unknown
			;;
		esac
		shift
	done
}

function common_check_requirements {
	if [[ ! -d $ANDROID_NDK ]];
	then
		echo "export ANDROID_NDK to point to your NDK location."
		exit 1
	fi

	if [[ -z $BUILD_DST ]];
	then
		echo "Destination directory not valid"
		exit 1
	fi

	if [[ -z $BUILD_SRC ]];
	then
		echo "Source directory not valid"
		exit 1
	fi

	if [[ -z $SCM_URL ]];
	then
		echo "Source URL not defined! Define SCM_URL"
		exit 1
	fi

	if [[ -z $SCM_TAG ]];
	then
		echo "SCM TAG / BRANCH not defined! Define SCM_TAG"
		exit 1
	fi

	if [[ -z $NDK_TARGET ]];
	then
		echo "Android platform NDK_TARGET not defined"
		exit 1
	fi

	if [ -x $ANDROID_NDK/ndk-build ]; then
		NDK_BUILD=$ANDROID_NDK/ndk-build
	else
		echo "ndk-build not found in NDK directory $ANDROID_NDK"
		echo "assuming ndk-build is in path..."
		NDK_BUILD=ndk-build
	fi

	if [[ -z $NDK_BUILD ]];
	then
		echo "Android ndk-build not detected"
		exit 1
	fi

	for CMD in make git cmake $NDK_BUILD
	do
		if ! type $CMD >/dev/null; then
			echo "Command $CMD not found. Install and add it to the PATH."
			exit 1
		fi
	done

	if [ "${BUILD_SRC:0:1}" != "/" ];
	then
		BUILD_SRC=$(pwd)/$BUILD_SRC
	fi
	if [ "${BUILD_DST:0:1}" != "/" ];
	then
		BUILD_DST=$(pwd)/$BUILD_DST
	fi
}

function common_update {
	if [ $# -ne 3 ];
	then
		echo "Invalid arguments to update function $@"
		exit 1
	fi

	echo "Preparing checkout..."
	BASE=$(pwd)
	if [[ ! -d $3 ]];
	then
		common_run mkdir -p $3
		common_run cd $3
		common_run git clone $1 $3
	fi

	common_run cd $BASE
	common_run cd $3
	common_run git fetch
	common_run git reset --hard HEAD
	common_run git checkout $2
	common_run cd $BASE
}

function common_clean {
	if [ $CLEAN_BUILD_DIR -ne 1 ];
	then
		return
	fi

	if [ $# -ne 1 ];
	then
		echo "Invalid arguments to clean function $@"
		exit 1
	fi

	echo "Cleaning up $1..."
	common_run rm -rf $1
}

function common_copy {
	if [ $# -ne 2 ];
	then
		echo "Invalid arguments to copy function $@"
		exit 1
	fi
	if [ ! -d $1 ] || [ ! -d $1/include ] || [ ! -d $1/libs ];
	then
		echo "Invalid source $1"
		exit 1
	fi
	if [ -z $2 ];
	then
		echo "Invalid destination $2"
		exit 1
	fi

	if [ ! -d $2 ];
	then
		common_run mkdir -p $2
	fi
	common_run cp -L -r $1/include $2
	common_run cp -L -r $1/libs/* $2
}
