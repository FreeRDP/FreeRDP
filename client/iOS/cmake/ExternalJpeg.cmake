include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  jpeg
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/jpeg
  URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEG_VERSION}/libjpeg-turbo-${JPEG_VERSION}.tar.gz
  URL_HASH ${JPEG_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  CMAKE_ARGS ${IOS_CMAKE_ARGS}
             -DCMAKE_INSTALL_PREFIX:PATH=${DEPS_INSTALL_DIR}
             -DCMAKE_INSTALL_LIBDIR:STRING=${CMAKE_INSTALL_LIBDIR}
             -DWITH_TURBOJPEG:BOOL=OFF
             -DENABLE_TESTING:BOOL=OFF
             -DWITH_TOOLS:BOOL=OFF
)
