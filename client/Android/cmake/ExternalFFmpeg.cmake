include(ExternalProject)

set(FFMPEG_VERSION "n8.1")

# Map Android ABI to FFmpeg cross-compile names
if(ANDROID_ABI STREQUAL "arm64-v8a")
  set(FFMPEG_ARCH "aarch64")
  set(FFMPEG_CPU "armv8-a")
  set(FFMPEG_ASM_FLAGS "--enable-neon;--enable-asm;--enable-inline-asm")
  set(FFMPEG_LDFLAGS "-march=armv8-a")
  set(FFMPEG_BIGENDIAN "no")
  set(FFMPEG_CLANG_TARGET "aarch64-linux-android${NDK_API_LEVEL}")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
  set(FFMPEG_ARCH "arm")
  set(FFMPEG_CPU "armv7-a")
  set(FFMPEG_ASM_FLAGS "--enable-neon;--enable-asm;--enable-inline-asm")
  set(FFMPEG_LDFLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=softfp -Wl,--fix-cortex-a8")
  set(FFMPEG_BIGENDIAN "")
  set(FFMPEG_CLANG_TARGET "armv7a-linux-androideabi${NDK_API_LEVEL}")
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(FFMPEG_ARCH "x86_64")
  set(FFMPEG_CPU "x86-64")
  set(FFMPEG_ASM_FLAGS "--disable-neon;--enable-asm;--enable-inline-asm") # x86_64
  set(FFMPEG_LDFLAGS "-march=x86-64")
  set(FFMPEG_BIGENDIAN "")
  set(FFMPEG_CLANG_TARGET "x86_64-linux-android${NDK_API_LEVEL}")
elseif(ANDROID_ABI STREQUAL "x86")
  set(FFMPEG_ARCH "x86")
  set(FFMPEG_CPU "i686")
  set(FFMPEG_ASM_FLAGS "--disable-neon;--disable-asm;--disable-inline-asm") # FFmpeg issue #4928
  set(FFMPEG_LDFLAGS "-march=i686")
  set(FFMPEG_BIGENDIAN "")
  set(FFMPEG_CLANG_TARGET "i686-linux-android${NDK_API_LEVEL}")
else()
  message(FATAL_ERROR "ExternalFFmpeg: unsupported ABI '${ANDROID_ABI}'")
endif()

set(NDK_TOOLCHAIN_BIN "${NDK_ROOT}/toolchains/llvm/prebuilt/${NDK_HOST_PLATFORM}/bin")
set(NDK_SYSROOT "${NDK_ROOT}/toolchains/llvm/prebuilt/${NDK_HOST_PLATFORM}/sysroot")

set(FFMPEG_CC "${NDK_TOOLCHAIN_BIN}/${FFMPEG_CLANG_TARGET}-clang")
set(FFMPEG_CXX "${NDK_TOOLCHAIN_BIN}/${FFMPEG_CLANG_TARGET}-clang++")

find_package(PkgConfig REQUIRED)

# Conditionally add ac_cv_c_bigendian env var (arm64 only)
if(FFMPEG_BIGENDIAN STREQUAL "no")
  set(FFMPEG_BIGENDIAN_ENV "ac_cv_c_bigendian=no")
else()
  set(FFMPEG_BIGENDIAN_ENV "")
endif()

ExternalProject_Add(
  ffmpeg
  INSTALL_DIR ${DEPS_INSTALL_DIR}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/ffmpeg
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/ffmpeg
  URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/${FFMPEG_VERSION}.tar.gz
  URL_HASH SHA256=dd308201bb1239a1b73185f80c6b4121f4efdfa424a009ce544fd00bf736bb2e
  DEPENDS openh264 opus openssl
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E env "PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH}"
    "PKG_CONFIG_PATH=${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig" ${FFMPEG_BIGENDIAN_ENV}
    <SOURCE_DIR>/configure --cross-prefix=${NDK_TOOLCHAIN_BIN}/llvm- --cc=${FFMPEG_CC} --cxx=${FFMPEG_CXX}
    --pkg-config=${PKG_CONFIG_EXECUTABLE} --sysroot=${NDK_SYSROOT} --arch=${FFMPEG_ARCH} --cpu=${FFMPEG_CPU}
    --extra-ldflags=${FFMPEG_LDFLAGS} --enable-libopus --enable-libopenh264 --target-os=android --enable-cross-compile
    --enable-pic --enable-lto --enable-jni --enable-mediacodec --enable-shared --disable-static --disable-programs
    --disable-doc --prefix=${DEPS_INSTALL_DIR} --libdir=${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR} ${FFMPEG_ASM_FLAGS}
  BUILD_COMMAND ${CMAKE_COMMAND} -E env "PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH}" make -j
  INSTALL_COMMAND ${CMAKE_COMMAND} -E env "PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH}" make -j install
)
