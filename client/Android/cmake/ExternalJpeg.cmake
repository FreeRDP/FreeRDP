include(ExternalProject)
include(DepVersions)

if(ANDROID_ABI STREQUAL "riscv64")
  set(TURBO_ARGS "-DWITH_SIMD=OFF")
endif()

ExternalProject_Add(
  jpeg
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/jpeg
  URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEG_VERSION}/libjpeg-turbo-${JPEG_VERSION}.tar.gz
  URL_HASH ${JPEG_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  LIST_SEPARATOR |
  CMAKE_ARGS ${ANDROID_CMAKE_ARGS} -DWITH_TURBOJPEG:BOOL=OFF -DENABLE_TESTING:BOOL=OFF -DWITH_TOOLS:BOOL=OFF
             ${TURBO_ARGS}
)
