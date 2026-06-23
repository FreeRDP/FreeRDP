include(ExternalProject)
include(DepVersions)

set(_ff_arch "arm64")

# for simulator
if(PLATFORM MATCHES "SIMULATOR")
  set(_ff_sdk "iphonesimulator")
  set(_ff_min "-mios-simulator-version-min=${DEPLOYMENT_TARGET}")
  set(_ff_triple "-target ${_ff_arch}-apple-ios${DEPLOYMENT_TARGET}-simulator")
else()
  set(_ff_sdk "iphoneos")
  set(_ff_min "-miphoneos-version-min=${DEPLOYMENT_TARGET}")
  set(_ff_triple "")
endif()

execute_process(COMMAND xcrun --sdk ${_ff_sdk} --show-sdk-path OUTPUT_VARIABLE _ff_sysroot OUTPUT_STRIP_TRAILING_WHITESPACE)

set(_ff_flags "-arch ${_ff_arch} ${_ff_min} -isysroot ${_ff_sysroot} ${_ff_triple}")

ExternalProject_Add(
  ffmpeg
  PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  SOURCE_DIR ${CMAKE_BINARY_DIR}/external/ffmpeg
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/ffmpeg
  URL https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz
  URL_HASH ${FFMPEG_HASH}
  CONFIGURE_COMMAND
    <SOURCE_DIR>/configure --prefix=${DEPS_INSTALL_DIR} --enable-cross-compile --target-os=darwin
    --arch=${_ff_arch} --cc=clang --sysroot=${_ff_sysroot} "--extra-cflags=${_ff_flags}"
    "--extra-ldflags=${_ff_flags}" --disable-gpl --enable-static --disable-shared --enable-pic
    --disable-x86asm --disable-programs --disable-doc --disable-debug --disable-network
    --disable-autodetect --disable-everything --disable-avdevice --disable-avformat --disable-avfilter
    --disable-swscale --disable-swresample --disable-postproc --enable-avcodec --enable-avutil
    --enable-decoder=h264 --enable-parser=h264 --enable-decoder=hevc --enable-parser=hevc
    --enable-videotoolbox --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox
  BUILD_COMMAND make -j
  INSTALL_COMMAND make -j install
  BUILD_BYPRODUCTS ${DEPS_INSTALL_DIR}/lib/libavcodec.a ${DEPS_INSTALL_DIR}/lib/libavutil.a
)
