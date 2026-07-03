include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  libpng URL https://github.com/pnggroup/libpng/archive/refs/tags/${PNG_VERSION}.tar.gz URL_HASH ${PNG_HASH}
  LIST_SEPARATOR | DOWNLOAD_EXTRACT_TIMESTAMP OFF CMAKE_CACHE_ARGS ${IOS_CMAKE_CACHE_ARGS}
  CMAKE_ARGS ${IOS_CMAKE_ARGS} -DPNG_TESTS:BOOL=OFF -DPNG_TOOLS:BOOL=OFF
)
