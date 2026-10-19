#!/bin/bash -xe
#
# Emscripten (WebAssembly) build script
#
# Builds the dependencies and the SDL3 client for the browser (static,
# pthreads), and with -t also the unit tests, which then run in node.
# Run it where emcmake and emcc are in PATH, e.g. in the emscripten/emsdk
# docker image. Needs cmake, ninja and git.
# Needs emsdk 6 (CI uses 6.0.12): with 4.x the unit tests abort in
# emscripten_sleep(), which needs ASYNCIFY there.
#
# The client needs a page that serves sdl-freerdp.js with
#   Cross-Origin-Opener-Policy: same-origin
#   Cross-Origin-Embedder-Policy: require-corp
# (SharedArrayBuffer for pthreads) and a WebSocket to TCP bridge such as
# websockify in front of the RDP server.
#
# TODO: Replace with CMake FetchContent
#       https://cmake.org/cmake/help/latest/module/FetchContent.html
#
SCRIPT_PATH=$(dirname "${BASH_SOURCE[0]}")
SCRIPT_PATH=$(realpath "$SCRIPT_PATH")

SRC_BASE="$SCRIPT_PATH/../build-emscripten/src"
BUILD_BASE="$SCRIPT_PATH/../build-emscripten/build"
INSTALL_BASE="$SCRIPT_PATH/../build-emscripten/install"

CLEAN=0
CLONE=1
DEPS=1
BUILD=1
TESTS=0

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
  -t | --with-tests)
    TESTS=1
    ;;
  *)
    # unknown option
    echo "unknown option '$i', quit"
    echo "usage:\n\t$0 [-b|--no-build] [-c|--no-clone] [-d|--no-deps] [-t|--with-tests] [--clean-first]"
    exit 1
    ;;
  esac
done

if [ $CLEAN -ne 0 ]; then
  rm -rf "$BUILD_BASE"
  rm -rf "$INSTALL_BASE"
fi

# Shared memory (pthreads) needs every object compiled with -pthread;
# keep any flags given by the caller
export CFLAGS="${CFLAGS:+$CFLAGS }-pthread"
export CXXFLAGS="${CXXFLAGS:+$CXXFLAGS }-pthread"
export LDFLAGS="${LDFLAGS:+$LDFLAGS }-pthread"

function do_clone {
  version=$1
  url=$2
  dir=$3

  if [ -d "$dir" ]; then
    (
      cd "$dir"
      # a shallow single-branch clone does not see other tags with --all
      git fetch --depth 1 origin tag $version
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
  emcmake cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$INSTALL_BASE" \
    -DCMAKE_FIND_ROOT_PATH="$INSTALL_BASE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_BASE" \
    -DBUILD_SHARED_LIBS=OFF \
    -B "$1" \
    "${@:2}"
  cmake --build "$1"
}

mkdir -p "$SRC_BASE"
mkdir -p "$BUILD_BASE"

cd "$SRC_BASE"
if [ $CLONE -ne 0 ]; then
  do_clone v1.3.2 https://github.com/madler/zlib.git zlib
  do_clone uriparser-1.0.2 https://github.com/uriparser/uriparser.git uriparser
  do_clone json-c-0.19-20260627 https://github.com/json-c/json-c.git json-c
  do_clone release-3.4.18 https://github.com/libsdl-org/SDL.git SDL
  do_clone release-3.2.2 https://github.com/libsdl-org/SDL_ttf.git SDL_ttf
  do_clone v1.6.1 https://github.com/xiph/opus.git opus
  do_download https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/ libressl-4.3.3.tar.gz ff97c432457f349e6ba3d416ab903bc7468f1436f0f32efe5fff808de292c7b8 libressl
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
  cmake --install "$BUILD_BASE/libressl"

  do_cmake_build \
    "$BUILD_BASE/zlib" \
    -S zlib \
    -DZLIB_BUILD_SHARED=OFF \
    -DZLIB_BUILD_TESTING=OFF
  cmake --install "$BUILD_BASE/zlib"

  do_cmake_build \
    "$BUILD_BASE/uriparser" \
    -S uriparser \
    -DURIPARSER_BUILD_TOOLS=OFF \
    -DURIPARSER_BUILD_DOCS=OFF \
    -DURIPARSER_BUILD_TESTS=OFF
  cmake --install "$BUILD_BASE/uriparser"

  do_cmake_build \
    "$BUILD_BASE/json-c" \
    -S json-c \
    -DBUILD_APPS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DDISABLE_THREAD_LOCAL_STORAGE=ON
  cmake --install "$BUILD_BASE/json-c"

  do_cmake_build \
    "$BUILD_BASE/SDL" \
    -S SDL \
    -DSDL_TEST_LIBRARY=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_PTHREADS=ON
  cmake --install "$BUILD_BASE/SDL"

  do_cmake_build \
    "$BUILD_BASE/SDL_ttf" \
    -S SDL_ttf \
    -DSDLTTF_HARFBUZZ=ON \
    -DSDLTTF_FREETYPE=ON \
    -DSDLTTF_VENDORED=ON \
    -DSDLTTF_SAMPLES=OFF \
    -DSDLTTF_PLUTOSVG=OFF
  cmake --install "$BUILD_BASE/SDL_ttf"

  do_cmake_build \
    "$BUILD_BASE/opus" \
    -S opus \
    -DOPUS_BUILD_PROGRAMS=OFF \
    -DOPUS_BUILD_TESTING=OFF \
    -DOPUS_DISABLE_INTRINSICS=ON
  cmake --install "$BUILD_BASE/opus"
fi

# Features without a backend in the browser
FREERDP_ARGS=(
  -S "$SCRIPT_PATH/.."
  -DWITHOUT_WINPR_3x_DEPRECATED=ON
  -DWITHOUT_FREERDP_3x_DEPRECATED=ON
  -DWITH_SERVER=OFF
  -DWITH_SHADOW=OFF
  -DWITH_PROXY=OFF
  -DWITH_SAMPLE=OFF
  -DWITH_PLATFORM_SERVER=OFF
  -DWITH_X11=OFF
  -DWITH_WAYLAND=OFF
  -DWITH_ALSA=OFF
  -DWITH_PULSE=OFF
  -DWITH_OSS=OFF
  -DWITH_CUPS=OFF
  -DWITH_PCSC=OFF
  -DWITH_KRB5=OFF
  -DWITH_FUSE=OFF
  -DWITH_LIBSYSTEMD=OFF
  -DWITH_FFMPEG=OFF
  -DWITH_SWSCALE=OFF
  -DWITH_DSP_FFMPEG=OFF
  -DWITH_OPENH264=OFF
  -DWITH_WEBVIEW=OFF
  -DCHANNEL_URBDRC=OFF
  -DWITH_SIMD=OFF
  -DWITH_MANPAGES=OFF
  -DWITH_TIMEZONE_ICU=OFF
  -DWITH_UNICODE_BUILTIN=ON
  -DUSE_UNWIND=OFF
  -DWITH_LIBRESSL=ON
  -DWITH_OPUS=ON
  -DWITH_JSONC_REQUIRED=ON
)

# The client runs on the main browser thread and has to yield to the event
# loop while it waits (ASYNCIFY); the page starts it with callMain().
do_cmake_build \
  "$BUILD_BASE/freerdp" \
  "${FREERDP_ARGS[@]}" \
  -DWITH_CLIENT_SDL=ON \
  -DWITH_CLIENT_SDL3=ON \
  -DWITH_CLIENT_SDL2=OFF \
  -DWITH_SDL_LINK_SHARED=OFF \
  -DWITH_SDL_IMAGE_DIALOGS=OFF \
  -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sPTHREAD_POOL_SIZE=32 -sWEBSOCKET_SUBPROTOCOL=binary -sENVIRONMENT=web,worker -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS,ENV"

if [ $TESTS -ne 0 ]; then
  # The unit tests run in node; main() runs on a pthread so that the tests
  # can block while waiting for other threads.
  do_cmake_build \
    "$BUILD_BASE/freerdp-tests" \
    "${FREERDP_ARGS[@]}" \
    -DWITH_CLIENT=OFF \
    -DBUILD_TESTING=ON \
    -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS -sPROXY_TO_PTHREAD -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2147483648 -sSTACK_SIZE=4194304 -sPTHREAD_POOL_SIZE=8 -sEXIT_RUNTIME=1 -sENVIRONMENT=node -sNODERAWFS=1"

  # Not available in a browser or node: waitable timers (no timerfd or
  # kqueue), named pipes (no AF_UNIX sockets), dynamic libraries and processes
  ctest --test-dir "$BUILD_BASE/freerdp-tests" --output-on-failure --timeout 300 -j "$(nproc)" \
    -E '^(TestSynchWaitableTimer|TestSynchWaitableTimerAPC|TestPipeCreateNamedPipe|TestLibraryLoadLibrary|TestLibraryGetProcAddress|TestLibraryGetModuleFileName|TestThreadCreateProcess)$'
fi
