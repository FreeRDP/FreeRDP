#!/bin/bash

SCM_URL=https://github.com/openssl/openssl/releases/download/
SCM_TAG=openssl-3.5.0
SCM_HASH=344d0a79f1a9b08029b0744e2cc401a43f9c90acd1044d09a530b4885a8e9fc0

COMPILER=4.9

source $(dirname "${BASH_SOURCE[0]}")/android-build-common.sh

function build {
  if [ $# -ne 2 ]; then
    echo "Invalid arguments $@"
    exit 1
  fi

  CONFIG=$1
  DST_PREFIX=$2

  common_run export CC=clang
  common_run export PATH=$(${SCRIPT_PATH}/toolchains_path.py --ndk ${ANDROID_NDK}):$ORG_PATH
  common_run export ANDROID_NDK=${ANDROID_NDK}
  common_run export ANDROID_NDK_ROOT=${ANDROID_NDK}
  common_run export ANDROID_NDK_HOME=${ANDROID_NDK}

  echo "CONFIG=$CONFIG"
  echo "DST_PREFIX=$DST_PREFIX"
  echo "PATH=$PATH"

  BASE=$(pwd)
  DST_DIR=$BUILD_DST/$DST_PREFIX
  common_run cd $BUILD_SRC
  common_run ./Configure ${CONFIG} -U__ANDROID_API__ -D__ANDROID_API__=$NDK_TARGET
  common_run make SHLIB_EXT=.so -j build_libs

  if [ ! -d $DST_DIR ]; then
    common_run mkdir -p $DST_DIR
  fi

  common_run cp *.so $DST_DIR/
  common_run cd $BASE
}

# Run the main program.
common_parse_arguments $@

SCM_MOD_TAG=$SCM_TAG

# Workaround for naming of OpenSSL releases changing with every major version
case $SCM_TAG in OpenSSL_*)
  SCM_MOD_TAG=${SCM_TAG//OpenSSL_/openssl-}
  SCM_MOD_TAG=${SCM_MOD_TAG//_/.}
  ;;
esac
common_update "$SCM_URL/$SCM_TAG" $SCM_MOD_TAG $BUILD_SRC $SCM_HASH

ORG_PATH=$PATH
for ARCH in $BUILD_ARCH; do

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

if [ ! -d $BUILD_DST/$ARCH/include ]; then
  common_run mkdir -p $BUILD_DST/$ARCH/include
fi
common_run cp -L -R $BUILD_SRC/include/openssl $BUILD_DST/$ARCH/include/
