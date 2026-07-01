include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  webp
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz
  URL_HASH ${WEBP_HASH}
  LIST_SEPARATOR |
  CMAKE_CACHE_ARGS ${IOS_CMAKE_CACHE_ARGS}
  CMAKE_ARGS ${IOS_CMAKE_ARGS}
             -DWEBP_BUILD_CWEBP:BOOL=OFF
             -DWEBP_BUILD_DWEBP:BOOL=OFF
             -DWEBP_BUILD_GIF2WEBP:BOOL=OFF
             -DWEBP_BUILD_IMG2WEBP:BOOL=OFF
             -DWEBP_BUILD_VWEBP:BOOL=OFF
             -DWEBP_BUILD_WEBPINFO:BOOL=OFF
             -DWEBP_BUILD_WEBPMUX:BOOL=OFF
)

if(WITH_JPEG)
  add_dependencies(webp jpeg)
endif()
if(WITH_PNG)
  add_dependencies(webp libpng)
endif()
