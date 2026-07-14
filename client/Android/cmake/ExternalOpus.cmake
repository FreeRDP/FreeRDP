include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  opus SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/opus
  URL https://github.com/xiph/opus/releases/download/v${OPUS_VERSION}/opus-${OPUS_VERSION}.tar.gz URL_HASH ${OPUS_HASH}
  DOWNLOAD_EXTRACT_TIMESTAMP OFF LIST_SEPARATOR |
  CMAKE_ARGS ${ANDROID_CMAKE_ARGS} -DOPUS_BUILD_PROGRAMS:BOOL=OFF -DOPUS_BUILD_TESTING:BOOL=OFF
             -DOPUS_BUILD_SHARED_LIBRARY:BOOL=ON
)
