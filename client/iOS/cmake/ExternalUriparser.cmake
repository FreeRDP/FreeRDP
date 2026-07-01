include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  uriparser
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/uriparser/uriparser/releases/download/uriparser-${URIPARSER_VERSION}/uriparser-${URIPARSER_VERSION}.tar.xz
  URL_HASH ${URIPARSER_HASH}
  CMAKE_ARGS ${IOS_CMAKE_ARGS} -DURIPARSER_BUILD_DOCS:BOOL=OFF -DURIPARSER_BUILD_TESTS:BOOL=OFF
             -DURIPARSER_BUILD_TOOLS:BOOL=OFF
)
