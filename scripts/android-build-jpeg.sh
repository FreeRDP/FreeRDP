#!/bin/bash
SCM_URL=https://github.com/akallabeth/jpeg8d.git
SCM_TAG=master

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

function usage {
	echo $0 [arguments]
	echo "\tThe script checks out the OpenH264 git repository"
	echo "\tto a local source directory, builds and installs"
	echo "\tthe library for all architectures defined to"
	echo "\tthe destination directory."
	echo ""
	echo "\t[-s|--source-dir <path>]"
	echo "\t[-d|--destination-dir <path>]"
	echo "\t[-a|--arch <architectures>]"
	echo "\t[-t|--tag <tag or branch>]"
	echo "\t[--scm-url <url>]"
	echo "\t[--ndk <android NDK path>]"
	echo "\t[--sdk <android SDK path>]"
	exit 1
}

function build {
	echo "Building architectures $BUILD_ARCH..."
	BASE=$(pwd)
	common_run cd $BUILD_SRC
	common_run $NDK_BUILD V=1 APP_ABI="${BUILD_ARCH}" -j clean
	common_run $NDK_BUILD V=1 APP_ABI="${BUILD_ARCH}" -j
	common_run cd $BASE
}

# Run the main program.
common_parse_arguments $@
common_check_requirements
common_update $SCM_URL $SCM_TAG $BUILD_SRC
common_clean $BUILD_DST

build

common_copy $BUILD_SRC $BUILD_DST
