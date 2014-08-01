#!/bin/sh
#
# This script checks out or updates and builds third party libraries
# required for the android build.
#
# Specifically these are:
#  - OpenSSL
#  - Android NDK Profiler
#  - Jpeg library
#
# Usage:
#  android_setup_build_env.sh <source root>

OPENSSL_SCM=https://github.com/akallabeth/openssl-android.git
NDK_PROFILER_SCM=https://github.com/richq/android-ndk-profiler.git
JPEG_LIBRARY_SCM=https://github.com/akallabeth/jpeg8d.git

SCRIPT_NAME=`basename $0`

if [ $# -ne 1 ]; then

	echo "Missing command line argument, current directory as root."
	ROOT=`pwd`
	ROOT=$ROOT/external
else
	ROOT=`readlink -f $1`
fi

if [ ! -d $ROOT ]; then
	echo "Argument '$ROOT' is not a directory."
	exit -2
fi
echo "Using '$ROOT' as root."

echo "Preparing OpenSSL..."
OPENSSL_SRC=$ROOT/openssl-build
if [ -d $OPENSSL_SRC ]; then
	cd $OPENSSL_SRC
	git pull
	RETVAL=$?
else
	git clone $OPENSSL_SCM $OPENSSL_SRC
	RETVAL=$?
fi
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute git command [$RETVAL]"
	exit -3
fi
cd $OPENSSL_SRC
make clean
# The makefile has a bug, which aborts during
# first compilation. Rerun make to build the whole lib.
make
make
make
RETVAL=0 # TODO: Check, why 2 is returned.
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute make command [$RETVAL]"
	exit -4
fi
# Copy the created library to the default openssl directory,
# so that CMake will detect it automatically.
SSL_ROOT=`find $OPENSSL_SRC -type d -name "openssl-?.?.*"`
if [ -z "$SSL_ROOT" ]; then
	echo "OpenSSL was not build successfully, aborting."
	exit -42
fi
mkdir -p $SSL_ROOT/lib
cp $SSL_ROOT/*.a $SSL_ROOT/lib/

rm -f $ROOT/openssl
ln -s $SSL_ROOT $ROOT/openssl

echo "Preparing NDK profiler..."
NDK_PROFILER_SRC=$ROOT/android-ndk-profiler
if [ -d $NDK_PROFILER_SRC ]; then
	cd $NDK_PROFILER_SRC
	git pull
	RETVAL=$?
else
	git clone $NDK_PROFILER_SCM $NDK_PROFILER_SRC
	RETVAL=$?
fi
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute git command [$RETVAL]"
	exit -5
fi
cd $NDK_PROFILER_SRC
ndk-build V=1 APP_ABI=armeabi-v7a clean
ndk-build V=1 APP_ABI=armeabi-v7a
RETVAL=$?
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute ndk-build command [$RETVAL]"
	exit -6
fi

echo "Preparing JPEG library..."
JPEG_LIBRARY_SRC=$ROOT/jpeg8d
if [ -d $JPEG_LIBRARY_SRC ]; then
	cd $JPEG_LIBRARY_SRC
	git pull
	RETVAL=$?
else
	git clone $JPEG_LIBRARY_SCM $JPEG_LIBRARY_SRC
	RETVAL=$?
fi
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute git command [$RETVAL]"
	exit -6
fi
cd $JPEG_LIBRARY_SRC
ndk-build V=1 APP_ABI=armeabi-v7a clean
ndk-build V=1 APP_ABI=armeabi-v7a
RETVAL=$?
if [ $RETVAL -ne 0 ]; then
	echo "Failed to execute ndk-build command [$RETVAL]"
	exit -7
fi
mkdir -p $JPEG_LIBRARY_SRC/lib
cp $JPEG_LIBRARY_SRC/obj/local/armeabi-v7a/*.a $JPEG_LIBRARY_SRC/lib/

echo "Prepared external libraries, you can now build the application."
exit 0
