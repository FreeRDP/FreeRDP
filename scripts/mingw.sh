#!/bin/bash -xe

SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

SRC_BASE="$SCRIPT_PATH/../build-mingw/src"
BUILD_BASE="$SCRIPT_PATH/../build-mingw/build"
INSTALL_BASE="$SCRIPT_PATH/../build-mingw/install"

mkdir -p "$SRC_BASE"
mkdir -p "$BUILD_BASE"

cd "$SRC_BASE"
git clone -b v3.8.2 https://github.com/libressl/portable.git libressl
(
    cd libressl
    ./update.sh
)
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S libressl \
    -B "$BUILD_BASE/libressl" \
    -DLIBRESSL_APPS=OFF \
    -DLIBRESSL_TESTS=OFF
cmake --build "$BUILD_BASE/libressl"
cmake --install "$BUILD_BASE/libressl"

git clone --depth 1 -b v1.3.1 https://github.com/madler/zlib.git
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S zlib \
    -B "$BUILD_BASE/zlib"
cmake --build "$BUILD_BASE/zlib"
cmake --install "$BUILD_BASE/zlib"

git clone --depth 1 -b uriparser-0.9.7 https://github.com/uriparser/uriparser.git
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S uriparser \
    -B "$BUILD_BASE/uriparser" \
    -DURIPARSER_BUILD_DOCS=OFF \
    -DURIPARSER_BUILD_TESTS=OFF
cmake --build "$BUILD_BASE/uriparser"
cmake --install "$BUILD_BASE/uriparser"

git clone --depth 1 -b v1.7.17 https://github.com/DaveGamble/cJSON.git
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S cJSON \
    -B "$BUILD_BASE/cJSON" \
    -DENABLE_CJSON_TEST=OFF \
    -DBUILD_SHARED_AND_STATIC_LIBS=ON
cmake --build "$BUILD_BASE/cJSON"
cmake --install "$BUILD_BASE/cJSON"

git clone --depth 1 -b release-2.30.0 https://github.com/libsdl-org/SDL.git
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S SDL \
    -B "$BUILD_BASE/SDL" \
    -DSDL_TEST=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_STATIC_PIC=ON
cmake --build "$BUILD_BASE/SDL"
cmake --install "$BUILD_BASE/SDL"

git clone --depth 1 --shallow-submodules --recurse-submodules -b release-2.22.0 https://github.com/libsdl-org/SDL_ttf.git
cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -S SDL_ttf \
    -B "$BUILD_BASE/SDL_ttf" \
    -DSDL2TTF_HARFBUZZ=ON \
    -DSDL2TTF_FREETYPE=ON \
    -DSDL2TTF_VENDORED=ON \
    -DFT_DISABLE_ZLIB=OFF \
    -DSDL2TTF_SAMPLES=OFF
cmake --build "$BUILD_BASE/SDL_ttf"
cmake --install "$BUILD_BASE/SDL_ttf"

git clone --depth 1 --shallow-submodules --recurse-submodules -b release-2.8.2 https://github.com/libsdl-org/SDL_image.git
cmake \
     -GNinja \
     -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
     -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
     -S SDL_image \
     -B "$BUILD_BASE/SDL_image" \
     -DSDL2IMAGE_SAMPLES=OFF \
     -DSDL2IMAGE_DEPS_SHARED=OFF
cmake --build "$BUILD_BASE/SDL_image"
cmake --install "$BUILD_BASE/SDL_image"

git clone --depth 1 --shallow-submodules --recurse-submodules -b v1.0.27 https://github.com/libusb/libusb-cmake.git
cmake \
     -GNinja \
     -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
     -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
     -S libusb-cmake \
     -B "$BUILD_BASE/libusb-cmake" \
     -DLIBUSB_BUILD_EXAMPLES=OFF \
     -DLIBUSB_BUILD_TESTING=OFF \
     -DLIBUSB_ENABLE_DEBUG_LOGGING=OFF
cmake --build "$BUILD_BASE/libusb-cmake"
cmake --install "$BUILD_BASE/libusb-cmake"

# TODO: This takes ages to compile, disable
#git clone --depth 1 -b n6.1.1 https://github.com/FFmpeg/FFmpeg.git
#(
#    cd "$BUILD_BASE"
#    mkdir -p FFmpeg
#    cd FFmpeg
#    "$SRC_BASE/FFmpeg/configure" \
#        --arch=x86_64 \
#        --target-os=mingw64 \
#        --cross-prefix=x86_64-w64-mingw32- \
#        --prefix="$INSTALL_BASE"
#    make -j
#    make -j install
#)

git clone --depth 1 -b v2.4.1 https://github.com/cisco/openh264.git
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

cmake \
     -GNinja \
     -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/mingw64.cmake" \
     -DCMAKE_PREFIX_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_MODULE_PATH="$INSTALL_BASE/lib/cmake;$INSTALL_BASE/lib;$INSTALL_BASE" \
     -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
     -S "$SCRIPT_PATH/.." \
     -B "$BUILD_BASE/freerdp" \
     -DWITH_SERVER=ON \
     -DWITH_SHADOW=OFF \
     -DWITH_PLATFORM_SERVER=OFF \
     -DWITH_SAMPLE=ON \
     -DWITH_PLATFORM_SERVER=OFF \
     -DUSE_UNWIND=OFF \
     -DSDL_USE_COMPILED_RESOURCES=OFF \
     -DWITH_SWSCALE=OFF \
     -DWITH_FFMPEG=OFF \
     -DWITH_OPENH264=ON \
     -DWITH_WEBVIEW=OFF \
     -DWITH_LIBRESSL=ON \
     -DWITH_MANPAGES=OFF \
     -DZLIB_INCLUDE_DIR="$INSTALL_BASE/include" \
     -DZLIB_LIBRARY="$INSTALL_BASE/lib/libzlibstatic.a"
cmake --build "$BUILD_BASE/freerdp"
cmake --install "$BUILD_BASE/freerdp"
