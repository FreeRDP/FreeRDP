include(ExternalProject)
include(DepVersions)

set(_ff_arch "arm64")

if(PLATFORM MATCHES "SIMULATOR")
  set(_ff_sdk "iphonesimulator")
  set(_ff_min "-mios-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(_ff_triple "-target ${_ff_arch}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator")
  set(_ff_platform "iPhoneSimulator")
else()
  set(_ff_sdk "iphoneos")
  set(_ff_min "-miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(_ff_triple "")
  set(_ff_platform "iPhoneOS")
endif()

execute_process(
  COMMAND xcrun --sdk ${_ff_sdk} --show-sdk-path OUTPUT_VARIABLE _ff_sysroot OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(_ff_flags "-arch ${_ff_arch} ${_ff_min} -isysroot ${_ff_sysroot} ${_ff_triple}")

if(BUILD_SHARED_LIBS)
  set(EXTRA_ARGS --disable-static --enable-shared)
else()
  set(EXTRA_ARGS --enable-static --disable-shared)
endif()

ExternalProject_Add(
  ffmpeg
  PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  # Keep sources in the build tree; ExternalProject must not write into client/iOS.
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/ffmpeg
  URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/${FFMPEG_VERSION}.tar.gz
  URL_HASH ${FFMPEG_HASH}
  CONFIGURE_COMMAND
    <SOURCE_DIR>/configure --prefix=${DEPS_INSTALL_DIR} --enable-cross-compile --target-os=darwin --arch=${_ff_arch}
    --cc=clang --sysroot=${_ff_sysroot} "--extra-cflags=${_ff_flags}" "--extra-ldflags=${_ff_flags}" --disable-gpl
    ${EXTRA_ARGS} --enable-pic --disable-x86asm --disable-programs --disable-doc --disable-debug --disable-network
    --disable-autodetect --disable-everything --disable-avdevice --disable-avformat --disable-avfilter --enable-swscale
    --enable-swresample --enable-avcodec --enable-avutil --enable-decoder=h264 --enable-parser=h264
    --enable-decoder=hevc --enable-parser=hevc --enable-videotoolbox --enable-hwaccel=h264_videotoolbox
    --enable-hwaccel=hevc_videotoolbox --libdir=${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}
  BUILD_COMMAND make -j
  INSTALL_COMMAND make -j install
)
