#!/bin/sh
#
# This script checks out or updates and builds third party libraries
# required for the android build.
#
# Specifically these are:
#  - OpenSSL
#  - Android NDK Profiler
#
# Usage:
#  android_setup_build_env.sh <source root>

OPENSSL_SCM=https://github.com/bmiklautz/android-external-openssl-ndk-static
NDK_PROFILER_SCM=https://github.com/richq/android-ndk-profiler

SCRIPT_NAME=`basename $0`

if [ $# -ne 1 ]; then

	echo "Missing command line argument."
	echo "$SCRIPT_NAME <FreeRDP source>"
	exit -1
fi

if [ ! -d $1 ]; then
	echo "Argument '$1' is not a directory."
	exit -2
fi
SRC=`realpath $1`

echo "Using '$SRC' as root."

echo "Preparing OpenSSL..."
OPENSSL_SRC=$SRC/external/openssl
if [ -d $OPENSSL_SRC ]; then
	cd $OPENSSL_SRC
	git pull
	RETVAL=$?
else
	git clone $OPENSSL_SCM $OPENSSL_SRC
	RETVAL=$?
	cd $OPENSSL_SRC
fi
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute git command [$RETVAL]"
	exit -3
fi
ndk-build
RETVAL=$?
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute ndk-build command [$RETVAL]"
	exit -4
fi

echo "Preparing NDK profiler..."
NDK_PROFILER_SRC=$SRC/external/android-ndk-profiler
if [ -d $NDK_PROFILER_SRC ]; then
	cd $NDK_PROFILER_SRC
	git pull
	RETVAL=$?
else
	git clone $NDK_PROFILER_SCM $NDK_PROFILER_SRC
	RETVAL=$?
	cd $NDK_PROFILER_SRC
fi
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute git command [$RETVAL]"
	exit -5
fi
ndk-build
RETVAL=$?
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute ndk-build command [$RETVAL]"
	exit -6
fi

echo "Prepared external libraries, you can now build the application."
exit 0
