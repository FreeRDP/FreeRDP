include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  webp
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/webp
  URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz
  URL_HASH ${WEBP_HASH}
  CMAKE_ARGS ${ANDROID_CMAKE_ARGS}
             -DCMAKE_INSTALL_PREFIX:PATH=${DEPS_INSTALL_DIR}
             -DCMAKE_INSTALL_LIBDIR:STRING=${CMAKE_INSTALL_LIBDIR}
             -DWEBP_BUILD_CWEBP:BOOL=OFF
             -DWEBP_BUILD_DWEBP:BOOL=OFF
             -DWEBP_BUILD_GIF2WEBP:BOOL=OFF
             -DWEBP_BUILD_IMG2WEBP:BOOL=OFF
             -DWEBP_BUILD_VWEBP:BOOL=OFF
             -DWEBP_BUILD_WEBPINFO:BOOL=OFF
             -DWEBP_BUILD_WEBPMUX:BOOL=OFF
)
