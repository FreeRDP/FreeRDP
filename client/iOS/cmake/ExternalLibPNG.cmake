include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  libpng SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/libpng
  URL https://github.com/pnggroup/libpng/archive/refs/tags/${PNG_VERSION}.tar.gz URL_HASH ${PNG_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  CMAKE_ARGS ${IOS_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX:PATH=${DEPS_INSTALL_DIR}
             -DCMAKE_INSTALL_LIBDIR:STRING=${CMAKE_INSTALL_LIBDIR} -DPNG_TESTS:BOOL=OFF -DPNG_TOOLS:BOOL=OFF
)
