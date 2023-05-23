#!/bin/bash -xe
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

cmake \
    -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_PATH/../cmake/ClangToolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_LIBRARY_SUFFIXES=".a;.so" \
    -DBUILD_SHARED_LIBS=OFF \
    -Bclang \
    -S"$SCRIPT_PATH/.." \
    -DCMAKE_INSTALL_PREFIX=/tmp/xxx \
    -DWITH_SERVER=ON \
    -DWITH_SAMPLE=ON \
    -DWITH_CAIRO=ON \
    -DWITH_FFMPEG=ON \
    -DWITH_DSP_FFMPEG=ON \
    -DWITH_PKCS11=ON \
    -DWITH_SOZR=ON \
    -DWITH_WAYLAND=ON \
    -DWITH_WEBVIEW=ON \
    -DWITH_SWSCALE=ON
