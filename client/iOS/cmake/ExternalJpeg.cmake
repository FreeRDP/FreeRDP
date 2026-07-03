include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  jpeg
  URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEG_VERSION}/libjpeg-turbo-${JPEG_VERSION}.tar.gz
  URL_HASH ${JPEG_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  LIST_SEPARATOR |
  CMAKE_CACHE_ARGS ${IOS_CMAKE_CACHE_ARGS}
  CMAKE_ARGS ${IOS_CMAKE_ARGS} -DWITH_TURBOJPEG:BOOL=OFF -DENABLE_TESTING:BOOL=OFF -DWITH_TOOLS:BOOL=OFF
)
