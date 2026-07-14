include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  libpng SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/libpng
  URL https://github.com/pnggroup/libpng/archive/refs/tags/${PNG_VERSION}.tar.gz URL_HASH ${PNG_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF LIST_SEPARATOR | CMAKE_ARGS ${ANDROID_CMAKE_ARGS} -DPNG_TESTS:BOOL=OFF
                                                             -DPNG_TOOLS:BOOL=OFF
)
