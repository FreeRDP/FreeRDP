include(ExternalProject)

set(OPENSSL_VERSION "openssl-3.6.2")

# Map ANDROID_ABI to OpenSSL architecture names
if(ANDROID_ABI STREQUAL "arm64-v8a")
  set(OSSL_ARCH "android-arm64")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
  set(OSSL_ARCH "android-arm")
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(OSSL_ARCH "android-x86_64")
elseif(ANDROID_ABI STREQUAL "x86")
  set(OSSL_ARCH "android-x86")
else()
  message(FATAL_ERROR "ExternalOpenSSL: unsupported ABI '${ANDROID_ABI}'")
endif()

set(NDK_TOOLCHAIN_BIN "${NDK_ROOT}/toolchains/llvm/prebuilt/${NDK_HOST_PLATFORM}/bin")

ExternalProject_Add(
  openssl
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/openssl
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/openssl
  URL https://github.com/openssl/openssl/releases/download/${OPENSSL_VERSION}/${OPENSSL_VERSION}.tar.gz
  URL_HASH SHA256=aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E env PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH} ANDROID_NDK=${NDK_ROOT} ANDROID_NDK_ROOT=${NDK_ROOT}
    ANDROID_NDK_HOME=${NDK_ROOT} CC=clang perl <SOURCE_DIR>/Configure ${OSSL_ARCH} shared no-tests no-apps no-docs
    no-engine -U__ANDROID_API__ -D__ANDROID_API__=${NDK_API_LEVEL} --prefix=${DEPS_INSTALL_DIR}
    --libdir=${CMAKE_INSTALL_LIBDIR}
  BUILD_COMMAND ${CMAKE_COMMAND} -E env PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH} ANDROID_NDK=${NDK_ROOT}
                ANDROID_NDK_ROOT=${NDK_ROOT} ANDROID_NDK_HOME=${NDK_ROOT} make -j SHLIB_EXT=.so build_libs
  INSTALL_COMMAND
    ${CMAKE_COMMAND} -E env PATH=${NDK_TOOLCHAIN_BIN}:$ENV{PATH} ANDROID_NDK=${NDK_ROOT} ANDROID_NDK_ROOT=${NDK_ROOT}
    ANDROID_NDK_HOME=${NDK_ROOT} make -j SHLIB_EXT=.so install_sw && ${CMAKE_COMMAND} -E copy_directory
    ${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/ossl-modules ${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}
)
