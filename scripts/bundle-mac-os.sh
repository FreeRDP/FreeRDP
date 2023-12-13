#!/bin/bash -xe
SCRIPT_PATH="$(dirname -- "${BASH_SOURCE[0]}")"            # relative
SCRIPT_PATH="$(cd -- "$SCRIPT_PATH" && pwd)"    # absolutized and normalized

BASE=$(pwd)
SRC="$BASE/src"
BUILD="$BASE/build"
INSTALL="$BASE/install/MacFreeRDP.app/Contents"

BINDIR=MacOS
LIBDIR=Frameworks
DATADIR=Resources

DEPLOYMENT_ARCH="arm64 x86_64"
DEPLOYMENT_TARGET=12

CMAKE_ARCHS=
OSSL_FLAGS="-mmacosx-version-min=$DEPLOYMENT_TARGET"
for $ARCH in $DEPLOYMENT_ARCH;
do
	OSSL_FLAGS="$OSSL_FLAGS -arch $ARCH"
	CMAKE_ARCHS="$CMAKE_ARCHS;$ARCH"
done

CMAKE_ARGS="-DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON \
	-DCMAKE_VERBOSE_MAKEFILE=ON \
	-DCMAKE_BUILD_TYPE=Release \
	-DBUILD_SHARED_LIBS=ON \
	-DCMAKE_OSX_ARCHITECTURES=$CMAKE_ARCHS \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=$DEPLOYMENT_TARGET \
	-DCMAKE_INSTALL_PREFIX='$INSTALL' \
	-DCMAKE_INSTALL_LIBDIR='$LIBDIR' \
	-DCMAKE_INSTALL_BINDIR='$BINDIR' \
	-DCMAKE_INSTALL_DATADIR='$DATADIR' \
	-DINSTALL_LIB_DIR='$INSTALL/$LIBDIR' \
	-DINSTALL_BIN_DIR='$INSTALL/$BINDIR' \
	-DCMAKE_PREFIX_PATH='$INSTALL;$INSTALL/$LIBDIR;$INSTALL/$LIBDIR/cmake' \
	-DCMAKE_IGNORE_PATH='/opt/local;/usr/local;/opt/homebrew;/Library;~/Library'
	"

if [ ! -d $SRC ];
then
	mkdir -p $SRC
	cd $SRC
	git clone -b openssl-3.2.0 https://github.com/openssl/openssl.git
	git clone --depth 1 -b v1.3 https://github.com/madler/zlib.git
	git clone --depth 1 -b uriparser-0.9.7 https://github.com/uriparser/uriparser.git
	git clone --depth 1 -b v1.7.16 https://github.com/DaveGamble/cJSON.git
	git clone --depth 1 -b release-2.28.1 https://github.com/libsdl-org/SDL.git
	git clone --depth 1 --shallow-submodules --recurse-submodules -b release-2.20.2 https://github.com/libsdl-org/SDL_ttf.git
	git clone --depth 1 --shallow-submodules --recurse-submodules -b v1.0.26 https://github.com/libusb/libusb-cmake.git
	git clone --depth 1 -b n6.0 https://github.com/FFmpeg/FFmpeg.git
	git clone --depth 1 -b v2.4.0 https://github.com/cisco/openh264.git
fi

if [ -d $INSTALL ];
then
	rm -rf $INSTALL
fi

if [ -d $BUILD ];
then
	rm -rf $BUILD
fi

mkdir -p $BUILD
cd $BUILD

cmake -GNinja -Bzlib -S$SRC/zlib $CMAKE_ARGS
cmake --build zlib
cmake --install zlib

cmake -GNinja -Buriparser -S$SRC/uriparser $CMAKE_ARGS -DURIPARSER_BUILD_DOCS=OFF -DURIPARSER_BUILD_TESTS=OFF
cmake --build uriparser
cmake --install uriparser

cmake -GNinja -BcJSON -S$SRC/cJSON $CMAKE_ARGS -DENABLE_CJSON_TEST=OFF -DBUILD_SHARED_AND_STATIC_LIBS=OFF 
cmake --build cJSON
cmake --install cJSON

cmake -GNinja -BSDL -S$SRC/SDL $CMAKE_ARGS -DSDL_TEST=OFF -DSDL_TESTS=OFF -DSDL_STATIC_PIC=ON 
cmake --build SDL
cmake --install SDL

cmake -GNinja -BSDL_ttf -S$SRC/SDL_ttf $CMAKE_ARGS -DSDL2TTF_HARFBUZZ=ON -DSDL2TTF_FREETYPE=ON -DSDL2TTF_VENDORED=ON \
    -DFT_DISABLE_ZLIB=OFF -DSDL2TTF_SAMPLES=OFF
cmake --build SDL_ttf
cmake --install SDL_ttf

cmake -GNinja -Blibusb-cmake -S$SRC/libusb-cmake $CMAKE_ARGS -DLIBUSB_BUILD_EXAMPLES=OFF -DLIBUSB_BUILD_TESTING=OFF \
	-DLIBUSB_ENABLE_DEBUG_LOGGING=OFF -DLIBUSB_BUILD_SHARED_LIBS=ON
cmake --build libusb-cmake
cmake --install libusb-cmake

mkdir -p openssl
cd openssl

CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS $SRC/openssl/config --prefix=$INSTALL --libdir=$LIBDIR no-asm no-tests no-docs no-apps zlib
CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS make -j build_sw
CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS make -j install_sw

cd $BUILD

meson setup --prefix="$INSTALL" -Doptimization=3 -Db_lto=true -Db_pie=true -Dc_args="$OSSL_FLAGS" -Dc_link_args="$OSSL_FLAGS" \
	-Dcpp_args="$OSSL_FLAGS" -Dcpp_link_args="$OSSL_FLAGS" -Dpkgconfig.relocatable=true -Dtests=disabled -Dbindir=$BINDIR \
       	-Dlibdir=$LIBDIR openh264 $SRC/openh264
ninja -C openh264 install

cmake -GNinja -Bfreerdp -S"$SCRIPT_PATH/.." $CMAKE_ARGS -DWITH_PLATFORM_SERVER=OFF -DWITH_NEON=OFF -DWITH_SSE=OFF -DWITH_FFMPEG=OFF \
	-DWITH_SWSCALE=OFF -DWITH_OPUS=OFF -DWITH_WEBVIEW=OFF
cmake --build freerdp
cmake --install freerdp

# some build systems do not handle @rpath on mac os correctly.
# do check that and fix it.
DYLIB_ABS_NAMES=$(find $INSTALL/$LIBDIR -name "*.dylib")
for DYLIB_ABS in $DYLIB_ABS_NAMES;
do
	DYLIB_NAME=$(basename $DYLIB_ABS)
	install_name_tool -id @rpath/$DYLIB_NAME $DYLIB_ABS

	for DYLIB_DEP in $(otool -L $DYLIB_ABS | grep "$INSTALL/$LIBDIR" | cut -d' ' -f1);
	do
		if [[ $DYLIB_DEP == $DYLIB_ABS ]];
		then
			continue
		elif [[ $DYLIB_DEP == $INSTALL/$LIBDIR/* ]];
		then
			DEP_BASE=$(basename $DYLIB_DEP)
			install_name_tool -change $DYLIB_DEP @rpath/$DEP_BASE $DYLIB_ABS
		fi
	done
done

# clean up unused data
rm -rf "$INSTALL/include"
rm -rf "$INSTALL/share"
rm -rf "$INSTALL/bin"
rm -rf "$INSTALL/$LIBDIR/cmake"
rm -rf "$INSTALL/$LIBDIR/pkgconfig"
rm -f "$INSTALL/$LIBDIR/*.a"

# TODO: Create remaining files required
