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

CLONE=1
DEPS=1
BUILD=1
FFMPEG=0

for i in "$@"
do
case $i in
    -b|--no-build)
    BUILD=0
    ;;
    -d|--no-deps)
    DEPS=0
    ;;
    -c|--no-clone)
    CLONE=0
    ;;
    -f|--with-ffmpeg)
    FFMPEG=1
    ;;
    *)
            # unknown option
	    echo "unknown option, quit"
	    echo "$0 [-d|--no-deps] [-c|--no-clone]"
	    exit 1
    ;;
esac
done

function do_clone {
	version=$1
	url=$2
	dir=$3

	if [ -d "$dir" ];
	then
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

function do_cmake_build {
	cmake \
	    -GNinja \
	    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
	    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
	    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
	    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
	    -B "$1" \
	    ${@:2}
	cmake --build "$1"
	cmake --install "$1"
}

mkdir -p "$SRC_BASE"
mkdir -p "$BUILD_BASE"

cd "$SRC_BASE"
if [ $CLONE -ne 0 ];
then
	do_clone v1.3.1 https://github.com/madler/zlib.git zlib
	do_clone uriparser-0.9.7 https://github.com/uriparser/uriparser.git uriparser
	do_clone v1.7.17 https://github.com/DaveGamble/cJSON.git cJSON
	do_clone release-2.30.0 https://github.com/libsdl-org/SDL.git SDL
	if [ $FFMPEG -ne 0 ];
	then
		do_clone n6.1.1 https://github.com/FFmpeg/FFmpeg.git FFmpeg
	fi
	do_clone v2.4.1 https://github.com/cisco/openh264.git openh264
	do_clone v1.0.27 https://github.com/libusb/libusb-cmake.git libusb-cmake
	do_clone release-2.8.2 https://github.com/libsdl-org/SDL_image.git SDL_image
	do_clone release-2.22.0 https://github.com/libsdl-org/SDL_ttf.git SDL_ttf
	do_clone v3.8.2 https://github.com/libressl/portable.git libressl
	(
	    cd libressl
	    ./update.sh
	)
fi

if [ $BUILD -eq 0 ];
then
	exit 0
fi

if [ $DEPS -ne 0 ];
then
	do_cmake_build \
	    "$BUILD_BASE/libressl" \
	    -S libressl \
	    -DLIBRESSL_APPS=OFF \
	    -DLIBRESSL_TESTS=OFF

	do_cmake_build \
	    "$BUILD_BASE/zlib" \
	    -S zlib

	do_cmake_build \
	    "$BUILD_BASE/uriparser" \
	    -S uriparser \
	    -DURIPARSER_BUILD_DOCS=OFF \
	    -DURIPARSER_BUILD_TESTS=OFF

	do_cmake_build \
	    "$BUILD_BASE/cJSON" \
	    -S cJSON \
	    -DENABLE_CJSON_TEST=OFF \
	    -DBUILD_SHARED_AND_STATIC_LIBS=ON

	do_cmake_build \
	    "$BUILD_BASE/SDL" \
	    -S SDL \
	    -DSDL_TEST=OFF \
	    -DSDL_TESTS=OFF \
	    -DSDL_STATIC_PIC=ON

	do_cmake_build \
	    "$BUILD_BASE/SDL_ttf" \
	    -S SDL_ttf \
	    -DSDL2TTF_HARFBUZZ=ON \
	    -DSDL2TTF_FREETYPE=ON \
	    -DSDL2TTF_VENDORED=ON \
	    -DFT_DISABLE_ZLIB=OFF \
	    -DSDL2TTF_SAMPLES=OFF

	do_cmake_build \
	     "$BUILD_BASE/SDL_image" \
	     -S SDL_image \
	     -DSDL2IMAGE_SAMPLES=OFF \
	     -DSDL2IMAGE_DEPS_SHARED=OFF

	do_cmake_build \
	     "$BUILD_BASE/libusb-cmake" \
	     -S libusb-cmake \
	     -DLIBUSB_BUILD_EXAMPLES=OFF \
	     -DLIBUSB_BUILD_TESTING=OFF \
	     -DLIBUSB_ENABLE_DEBUG_LOGGING=OFF

	# TODO: This takes ages to compile, disable
	if [ $FFMPEG -ne 0 ];
	then
	(
	    cd "$BUILD_BASE"
	    mkdir -p FFmpeg
	    cd FFmpeg
	    "$SRC_BASE/FFmpeg/configure" \
	        --arch=x86_64 \
	        --target-os=mingw64 \
	        --cross-prefix=x86_64-w64-mingw32- \
	        --prefix="$INSTALL_BASE"
	    make -j
	    make -j install
	)
	fi

	meson setup --cross-file "$SCRIPT_PATH/mingw-meson.conf" \
	    -Dprefix="$INSTALL_BASE" \
	    -Db_pie=true \
	    -Db_lto=true \
	    -Dbuildtype=release \
	    -Dtests=disabled \
	    -Ddefault_library=both \
	    "$BUILD_BASE/openh264" \
	    openh264
	ninja -C "$BUILD_BASE/openh264"
	ninja -C "$BUILD_BASE/openh264" install
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
     -DSDL_USE_COMPILED_RESOURCES=OFF \
     -DWITH_SWSCALE=OFF \
     -DWITH_FFMPEG=OFF \
     -DWITH_SIMD=ON \
     -DWITH_OPENH264=ON \
     -DWITH_WEBVIEW=OFF \
     -DWITH_LIBRESSL=ON \
     -DWITH_MANPAGES=OFF \
     -DZLIB_INCLUDE_DIR="$INSTALL_BASE/include" \
     -DZLIB_LIBRARY="$INSTALL_BASE/lib/libzlibstatic.a"
