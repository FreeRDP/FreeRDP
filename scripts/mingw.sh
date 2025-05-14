#!/bin/bash -xe
#
# mingw build script
#
# TODO: Replace with CMake FetchContent
#       https://cmake.org/cmake/help/latest/module/FetchContent.html
#
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

SRC_BASE="$SCRIPT_PATH/../build-mingw/src"
BUILD_BASE="$SCRIPT_PATH/../build-mingw/build"
INSTALL_BASE="$SCRIPT_PATH/../build-mingw/install"

CLEAN=0
CLONE=1
DEPS=1
BUILD=1
FFMPEG=0
OPENH264=0

ARG_SHARED=1
ARG_STATIC=0
CMAKE_DEFAULT_FLAGS=""
ARG_SHARED_MESON="-Ddefault_library=shared"
ARG_SHARED_FFMPEG="--disable-static --enable-shared"
for i in "$@"; do
  case $i in
  -b | --no-build)
    BUILD=0
    ;;
  -c | --no-clone)
    CLONE=0
    ;;
  --clean-first)
    CLEAN=1
    ;;
  -d | --no-deps)
    DEPS=0
    ;;
  -f | --with-ffmpeg)
    FFMPEG=1
    ;;
  -o | --with-openh264)
    OPENH264=1
    ;;
  -s | --static)
    ARG_SHARED=0
    ARG_STATIC=1
    CMAKE_DEFAULT_FLAGS="-static -static-libgcc -static-libstdc++"
    ARG_SHARED_MESON="-Ddefault_library=static"
    ARG_SHARED_FFMPEG=""
    ;;
  *)
    # unknown option
    echo "unknown option '$i', quit"
    echo "usage:\n\t$0 [-b|--no-build] [-c|--no-clone] [-d|--no-deps] [-f|--with-ffmpeg] [-o|--with-openh264] [-s|--static] [--clean-first]"
    exit 1
    ;;
  esac
done

ARG_COMPILED_RES=1
if [ $ARG_SHARED -ne 0 ]; then
  ARG_COMPILED_RES=0
fi

if [ $CLEAN -ne 0 ]; then
  rm -rf "$BUILD_BASE"
  rm -rf "$INSTALL_BASE"
fi

function do_clone {
  version=$1
  url=$2
  dir=$3

  if [ -d "$dir" ]; then
    (
      cd "$dir"
      git fetch --all
      git clean -xdf
      git reset --hard $version
      git checkout $version
      git submodule update --init --recursive
    )
  else
    git clone --depth 1 --shallow-submodules --recurse-submodules -b $version $url $dir
  fi
}

function do_download {
  url=$1
  file=$2
  hash=$3
  dir=$4

  if [ ! -f $file ]; then
    if command -v curl 2>&1 >/dev/null; then
      curl -o $file $url/$file
    elif command -v wget 2>&1 >/dev/null; then
      wget -O $file $url/$file
    else
      echo "Could not find wget or curl, exiting"
      exit 1
    fi
  fi

  if [ ! -f $file.sha256sum ]; then
    echo "$hash  $file" >$file.sha256sum
  fi

  sha256sum -c $file.sha256sum
  if [ -d $dir ]; then
    rm -rf $dir
  fi
  mkdir $dir
  tar xf $file --strip-components=1 -C $dir
}

function do_cmake_build {
  cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -DBUILD_SHARED_LIBS=$ARG_SHARED \
    -DCMAKE_EXE_LINKER_FLAGS="$CMAKE_DEFAULT_FLAGS" \
    -B "$1" \
    ${@:2}
  cmake --build "$1"
  cmake --install "$1"
}

mkdir -p "$SRC_BASE"
mkdir -p "$BUILD_BASE"

cd "$SRC_BASE"
if [ $CLONE -ne 0 ]; then
  do_clone v1.3.1 https://github.com/madler/zlib.git zlib
  do_clone uriparser-0.9.8 https://github.com/uriparser/uriparser.git uriparser
  do_clone json-c-0.18-20240915 https://github.com/json-c/json-c.git json-c
  do_clone release-3.2.10 https://github.com/libsdl-org/SDL.git SDL
  if [ $FFMPEG -ne 0 ]; then
    do_clone n7.1.1 https://github.com/FFmpeg/FFmpeg.git FFmpeg
  fi
  if [ $OPENH264 -ne 0 ]; then
    do_clone v2.6.0 https://github.com/cisco/openh264.git openh264
  fi
  do_clone v1.0.28-0 https://github.com/libusb/libusb-cmake.git libusb-cmake
  do_clone release-3.2.4 https://github.com/libsdl-org/SDL_image.git SDL_image
  do_clone release-3.2.2 https://github.com/libsdl-org/SDL_ttf.git SDL_ttf
  do_clone v2.0.3 https://github.com/mstorsjo/fdk-aac.git fdk-aac
  do_clone v1.5.2 https://gitlab.xiph.org/xiph/opus.git opus
  do_download https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/ libressl-4.0.0.tar.gz 4d841955f0acc3dfc71d0e3dd35f283af461222350e26843fea9731c0246a1e4 libressl
fi

if [ $BUILD -eq 0 ]; then
  exit 0
fi

if [ $DEPS -ne 0 ]; then
  do_cmake_build \
    "$BUILD_BASE/libressl" \
    -S libressl \
    -DLIBRESSL_APPS=OFF \
    -DLIBRESSL_TESTS=OFF

  do_cmake_build \
    "$BUILD_BASE/zlib" \
    -S zlib \
    -DZLIB_BUILD_EXAMPLES=OFF

  do_cmake_build \
    "$BUILD_BASE/uriparser" \
    -S uriparser \
    -DURIPARSER_BUILD_TOOLS=OFF \
    -DURIPARSER_BUILD_DOCS=OFF \
    -DURIPARSER_BUILD_TESTS=OFF

  do_cmake_build \
    "$BUILD_BASE/json-c" \
    -S json-c \
    -DBUILD_APPS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_SHARED_LIBS=$ARG_SHARED \
    -DBUILD_STATIC_LIBS=$ARG_STATIC

  do_cmake_build \
    "$BUILD_BASE/SDL" \
    -S SDL \
    -DSDL_TEST=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_STATIC_PIC=ON

  do_cmake_build \
    "$BUILD_BASE/SDL_ttf" \
    -S SDL_ttf \
    -DSDLTTF_HARFBUZZ=ON \
    -DSDLTTF_FREETYPE=ON \
    -DSDLTTF_VENDORED=ON \
    -DFT_DISABLE_ZLIB=OFF \
    -DSDLTTF_SAMPLES=OFF \
    -DSDLTTF_PLUTOSVG=OFF

  do_cmake_build \
    "$BUILD_BASE/SDL_image" \
    -S SDL_image \
    -DSDLIMAGE_SAMPLES=OFF \
    -DSDLIMAGE_DEPS_SHARED=$ARG_SHARED

  do_cmake_build \
    "$BUILD_BASE/libusb-cmake" \
    -S libusb-cmake \
    -DLIBUSB_BUILD_EXAMPLES=OFF \
    -DLIBUSB_BUILD_TESTING=OFF \
    -DLIBUSB_ENABLE_DEBUG_LOGGING=OFF

  do_cmake_build \
    "$BUILD_BASE/fdk-aac" \
    -S fdk-aac \
    -DBUILD_PROGRAMS=OFF

  do_cmake_build \
    "$BUILD_BASE/opus" \
    -S opus \
    -DOPUS_BUILD_SHARED_LIBRARY=$ARG_SHARED

  # TODO: This takes ages to compile, disable
  if [ $FFMPEG -ne 0 ]; then
    (
      cd "$BUILD_BASE"
      mkdir -p FFmpeg
      cd FFmpeg
      "$SRC_BASE/FFmpeg/configure" \
        --arch=x86_64 \
        --target-os=mingw64 \
        --cross-prefix=x86_64-w64-mingw32- \
        --disable-programs \
        --disable-doc \
        --prefix="$INSTALL_BASE" $ARG_SHARED_FFMPEG
      make -j
      make -j install
    )
  fi

  if [ $OPENH264 -ne 0 ]; then
    (
      meson setup --cross-file "$SCRIPT_PATH/mingw-meson.conf" \
        -Dprefix="$INSTALL_BASE" \
        -Db_pie=true \
        -Db_lto=true \
        -Dbuildtype=release \
        -Dtests=disabled \
        $ARG_SHARED_MESON \
        -Dcpp_link_args="$CMAKE_DEFAULT_FLAGS" \
        -Dcpp_args="$CMAKE_DEFAULT_FLAGS" \
        "$BUILD_BASE/openh264" \
        openh264
      ninja -C "$BUILD_BASE/openh264"
      ninja -C "$BUILD_BASE/openh264" install
    )
  fi
fi

do_cmake_build \
  "$BUILD_BASE/freerdp" \
  -S "$SCRIPT_PATH/.." \
  -DWITH_SERVER=ON \
  -DWITH_SHADOW=OFF \
  -DWITH_PLATFORM_SERVER=OFF \
  -DWITH_SAMPLE=ON \
  -DWITH_PLATFORM_SERVER=OFF \
  -DUSE_UNWIND=OFF \
  -DSDL_USE_COMPILED_RESOURCES=$ARG_COMPILED_RES \
  -DWITH_SDL_IMAGE_DIALOGS=ON \
  -DWITH_SDL_LINK_SHARED=$ARG_SHARED \
  -DWITH_CLIENT_SDL2=OFF \
  -DWITH_SWSCALE=$FFMPEG \
  -DWITH_FFMPEG=$FFMPEG \
  -DWITH_SIMD=ON \
  -DWITH_OPENH264=$OPENH264 \
  -DWITH_WEBVIEW=OFF \
  -DWITH_LIBRESSL=ON \
  -DWITH_OPUS=ON \
  -DWITH_JSONC_REQUIRED=ON \
  -DWITH_FDK_AAC=ON \
  -DWITH_MANPAGES=OFF
