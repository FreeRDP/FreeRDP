include(FetchContent)

set(URIPARSER_BUILD_DOCS OFF CACHE INTERNAL "fetch content")
set(URIPARSER_BUILD_TOOLS OFF CACHE INTERNAL "fetch content")
set(URIPARSER_BUILD_TESTS OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  uriparser
  URL      https://github.com/uriparser/uriparser/releases/download/uriparser-0.9.8/uriparser-0.9.8.tar.xz
  URL_HASH SHA256=1d71c054837ea32a31e462bce5a1af272379ecf511e33448e88100b87ff73b2e
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)

FetchContent_MakeAvailable(uriparser)
