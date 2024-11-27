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

DEPLOYMENT_ARCH='arm64 x86_64'
DEPLOYMENT_TARGET=12

usage () {
  echo "${BASH_SOURCE[0]} [-a|--arch 'arch1 arch2 ...'] [-t|--target target][-h|--help]"
  echo ""
  echo "default options:"
  echo "arch   [$DEPLOYMENT_ARCH]"
  echo "target [$DEPLOYMENT_TARGET]"
}

check_tools() {
    for TOOL in mkdir rm mv git dirname pwd find cut basename grep xargs cmake ninja autoconf automake aclocal autoheader glibtoolize lipo otool install_name_tool;
    do
        set +e
        TOOL_PATH=$(which "$TOOL")
        set -e
        echo "$TOOL: $TOOL_PATH"

        if [ ! -f "$TOOL_PATH" ];
        then
            echo "Missing $TOOL! please install and add to PATH."
            exit 1
        fi
    done
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -a|--arch)
      DEPLOYMENT_ARCH="$2"
      shift # past argument
      shift # past value
      ;;
    -t|--target)
      DEPLOYMENT_TARGET="$2"
      shift # past argument
      shift # past value
      ;;
    -t|--target)
      usage
      exit 0
      ;;
    -*|--*)
      usage
      exit 1
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

check_tools

fix_rpath() {
	SEARCH_PATH=$1
	FIX_PATH=$1
	EXT=".dylib"
	if [ "$#" -gt 1 ];
	then
		FIX_PATH=$2
	fi
	if [ "$#" -gt 2 ];
	then
		EXT=$3
	fi

  # some build systems do not handle @rpath on mac os correctly.
  # do check that and fix it.
  DYLIB_ABS_NAMES=$(find $SEARCH_PATH -type f -name "*$EXT")
  for DYLIB_ABS in $DYLIB_ABS_NAMES;
  do
  	DYLIB_NAME=$(basename $DYLIB_ABS)
  	install_name_tool -id @rpath/$DYLIB_NAME $DYLIB_ABS

  	for DYLIB_DEP in $(otool -L $DYLIB_ABS | grep "$FIX_PATH" | cut -d' ' -f1);
  	do
  		if [[ $DYLIB_DEP == $DYLIB_ABS ]];
  		then
  			continue
  		elif [[ $DYLIB_DEP == $FIX_PATH/* ]];
  		then
  			DEP_BASE=$(basename $DYLIB_DEP)
  			install_name_tool -change $DYLIB_DEP @rpath/$DEP_BASE $DYLIB_ABS
  		fi
  	done
  done
}

replace_rpath() {
	FILE=$1
	for PTH in $(otool -l $FILE | grep -A2 LC_RPATH  | grep path | xargs -J ' ' | cut -d ' ' -f2);
	do
		install_name_tool -delete_rpath $PTH $FILE
	done
	install_name_tool -add_rpath @loader_path/../$LIBDIR $FILE
}

CMAKE_ARCHS=
OSSL_FLAGS="-mmacosx-version-min=$DEPLOYMENT_TARGET -I$INSTALL/include -L$INSTALL/lib"
for ARCH in $DEPLOYMENT_ARCH;
do
	OSSL_FLAGS="$OSSL_FLAGS -arch $ARCH"
	CMAKE_ARCHS="$ARCH;$CMAKE_ARCHS"
done

echo "build arch   [$DEPLOYMENT_ARCH]"
echo "build target [$DEPLOYMENT_TARGET]"

CMAKE_ARGS="-DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=ON \
	-DCMAKE_VERBOSE_MAKEFILE=ON \
	-DCMAKE_BUILD_TYPE=Release \
 	-DWITH_MANPAGES=OFF \
	-DBUILD_SHARED_LIBS=ON \
	-DCMAKE_OSX_ARCHITECTURES=$CMAKE_ARCHS \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=$DEPLOYMENT_TARGET \
	-DCMAKE_INSTALL_PREFIX='$INSTALL' \
	-DCMAKE_INSTALL_LIBDIR='lib' \
	-DCMAKE_INSTALL_BINDIR='bin' \
	-DCMAKE_INSTALL_DATADIR='$DATADIR' \
	-DINSTALL_LIB_DIR='$INSTALL/lib' \
	-DINSTALL_BIN_DIR='$INSTALL/bin' \
	-DCMAKE_PREFIX_PATH='$INSTALL;$INSTALL/lib;$INSTALL/lib/cmake' \
	-DCMAKE_IGNORE_PATH='/opt/local;/usr/local;/opt/homebrew;/Library;~/Library'
	-DCMAKE_IGNORE_PREFIX_PATH='/opt/local;/usr/local;/opt/homebrew;/Library;~/Library'
	"

if [ ! -d $SRC ];
then
	mkdir -p $SRC
	cd $SRC
	git clone --depth 1 -b openssl-3.3.1 https://github.com/openssl/openssl.git
	git clone --depth 1 -b v1.3.1 https://github.com/madler/zlib.git
	git clone --depth 1 -b uriparser-0.9.8 https://github.com/uriparser/uriparser.git
	git clone --depth 1 -b v1.7.18 https://github.com/DaveGamble/cJSON.git
	git clone --depth 1 -b release-2.30.4 https://github.com/libsdl-org/SDL.git
	git clone --depth 1 --shallow-submodules --recurse-submodules -b release-2.22.0 https://github.com/libsdl-org/SDL_ttf.git
	git clone --depth 1 --shallow-submodules --recurse-submodules -b release-2.8.2 https://github.com/libsdl-org/SDL_image.git
	git clone --depth 1 --shallow-submodules --recurse-submodules -b v1.0.27-1 https://github.com/libusb/libusb-cmake.git
	git clone --depth 1 -b n7.0.1 https://github.com/FFmpeg/FFmpeg.git
	git clone --depth 1 -b v2.4.1 https://github.com/cisco/openh264.git
	git clone --depth 1 -b v1.5.2 https://gitlab.xiph.org/xiph/opus.git
	git clone --depth 1 -b 2.11.1 https://github.com/knik0/faad2.git
	git clone --depth 1 -b 1.18.0 https://gitlab.freedesktop.org/cairo/cairo.git
	git clone --depth 1 -b 1_30 https://github.com/knik0/faac.git
	cd faac
	./bootstrap
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

cmake -GNinja -Buriparser -S$SRC/uriparser $CMAKE_ARGS -DURIPARSER_BUILD_DOCS=OFF -DURIPARSER_BUILD_TESTS=OFF \
	-DURIPARSER_BUILD_TOOLS=OFF
cmake --build uriparser
cmake --install uriparser

cmake -GNinja -BcJSON -S$SRC/cJSON $CMAKE_ARGS -DENABLE_CJSON_TEST=OFF -DBUILD_SHARED_AND_STATIC_LIBS=OFF
cmake --build cJSON
cmake --install cJSON

cmake -GNinja -Bopus -S$SRC/opus $CMAKE_ARGS -DOPUS_BUILD_SHARED_LIBRARY=ON
cmake --build opus
cmake --install opus

cmake -GNinja -Bfaad2 -S$SRC/faad2 $CMAKE_ARGS
cmake --build faad2
cmake --install faad2

cmake -GNinja -BSDL -S$SRC/SDL $CMAKE_ARGS -DSDL_TEST=OFF -DSDL_TESTS=OFF -DSDL_STATIC_PIC=ON
cmake --build SDL
cmake --install SDL

cmake -GNinja -BSDL_ttf -S$SRC/SDL_ttf $CMAKE_ARGS -DSDL2TTF_HARFBUZZ=ON -DSDL2TTF_FREETYPE=ON -DSDL2TTF_VENDORED=ON \
    -DFT_DISABLE_ZLIB=OFF -DSDL2TTF_SAMPLES=OFF
cmake --build SDL_ttf
cmake --install SDL_ttf

cmake -GNinja -BSDL_image -S$SRC/SDL_image $CMAKE_ARGS -DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_DEPS_SHARED=OFF
cmake --build SDL_image
cmake --install SDL_image

cmake -GNinja -Blibusb-cmake -S$SRC/libusb-cmake $CMAKE_ARGS -DLIBUSB_BUILD_EXAMPLES=OFF -DLIBUSB_BUILD_TESTING=OFF \
	-DLIBUSB_ENABLE_DEBUG_LOGGING=OFF -DLIBUSB_BUILD_SHARED_LIBS=ON
cmake --build libusb-cmake
cmake --install libusb-cmake

mkdir -p openssl
cd openssl

CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS $SRC/openssl/config --prefix=$INSTALL --libdir=lib no-asm no-tests no-docs no-apps zlib
CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS make -j build_sw
CFLAGS=$OSSL_FLAGS LDFLAGS=$OSSL_FLAGS make -j install_sw

cd $BUILD
mkdir -p faac
cd faac
# undefine __SSE2__, symbol clashes with universal build
CFLAGS="$OSSL_FLAGS -U__SSE2__" LDFLAGS=$OSSL_FLAGS $SRC/faac/configure --prefix=$INSTALL --libdir="$INSTALL/lib" \
	--enable-shared --disable-static
CFLAGS="$OSSL_FLAGS -U__SSE2__" LDFLAGS=$OSSL_FLAGS make -j
CFLAGS="$OSSL_FLAGS -U__SSE2__" LDFLAGS=$OSSL_FLAGS make -j install

cd $BUILD

meson setup --prefix="$INSTALL" -Doptimization=3 -Db_lto=true -Db_pie=true -Dc_args="$OSSL_FLAGS" -Dc_link_args="$OSSL_FLAGS" \
	-Dcpp_args="$OSSL_FLAGS" -Dcpp_link_args="$OSSL_FLAGS" -Dpkgconfig.relocatable=true -Dtests=disabled \
       	-Dlibdir=lib openh264 $SRC/openh264
ninja -C openh264 install

for ARCH in $DEPLOYMENT_ARCH;
do
	mkdir -p $BUILD/FFmpeg/$ARCH
	cd $BUILD/FFmpeg/$ARCH
	FFCFLAGS="-arch $ARCH -mmacosx-version-min=$DEPLOYMENT_TARGET"
	FINSTPATH=$BUILD/FFmpeg/install/$ARCH
	CFLAGS=$FFCFLAGS LDFLAGS=$FFCFLAGS $SRC/FFmpeg/configure --prefix=$FINSTPATH --disable-all \
		--enable-shared --disable-static --enable-swscale --disable-asm --disable-libxcb \
		--disable-securetransport --disable-xlib --enable-cross-compile
	CFLAGS=$FFCFLAGS LDFLAGS=$FFCFLAGS make -j
	CFLAGS=$FFCFLAGS LDFLAGS=$FFCFLAGS make -j install
	fix_rpath "$FINSTPATH/lib"
done

BASE_ARCH="${DEPLOYMENT_ARCH%% *}"

cd $BUILD/FFmpeg/install/$ARCH
cp -r include/* $INSTALL/include/
find lib -type l -exec cp -P {} $INSTALL/lib/ \;
BASE_LIBS=$(find lib -type f -name "*.dylib" -exec basename {} \;)

cd $BUILD/FFmpeg/install
for LIB in $BASE_LIBS;
do
	LIBS=$(find . -name $LIB)
	lipo $LIBS -output $INSTALL/lib/$LIB -create
done

cd $BUILD
cmake -GNinja -Bfreerdp -S"$SCRIPT_PATH/.." \
	$CMAKE_ARGS \
	-DWITH_PLATFORM_SERVER=OFF \
	-DWITH_SIMD=ON \
	-DWITH_FFMPEG=OFF \
	-DWITH_SWSCALE=ON \
	-DWITH_OPUS=ON \
	-DWITH_WEBVIEW=OFF \
	-DWITH_FAAD2=ON \
	-DWITH_FAAC=ON \
	-DWITH_INTERNAL_RC4=ON \
	-DWITH_INTERNAL_MD4=ON \
	-DWITH_INTERNAL_MD5=ON \
	-DCHANNEL_RDPEAR=OFF \
	-DWITH_CJSON_REQUIRED=ON

cmake --build freerdp
cmake --install freerdp

# remove unused stuff from bin
find "$INSTALL"  -name "*.a" -exec rm -f {} \;
find "$INSTALL"  -name "*.la" -exec rm -f {} \;
find "$INSTALL" -name sdl2-config -exec rm -f {} \;

fix_rpath "$INSTALL/lib"
fix_rpath "$INSTALL/bin" "$INSTALL/lib" ""

# move files in place
cd $INSTALL
mv lib $LIBDIR
mv bin $BINDIR

# update RPATH
for LIB in $(find $LIBDIR -type f -name "*.dylib");
do
	replace_rpath $LIB
done

for BIN in $(find $BINDIR -type f);
do
	replace_rpath $BIN
done

# clean up unused data
rm -rf "$INSTALL/include"
rm -rf "$INSTALL/share"
rm -rf "$INSTALL/bin"
rm -rf "$INSTALL/$LIBDIR/cmake"
rm -rf "$INSTALL/$LIBDIR/pkgconfig"

# TODO: Create remaining files required
