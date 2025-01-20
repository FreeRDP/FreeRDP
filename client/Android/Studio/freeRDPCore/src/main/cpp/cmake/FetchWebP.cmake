include(FetchContent)

set(WEBP_BUILD_WEBMUX OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_CWEBP OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_DWEBP OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_GIF2WEBP OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_IMG2WEBP OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_WEBPINFO OFF CACHE INTERNAL "fetch content")
set(WEBP_BUILD_WEBPMUX OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  webp
  OVERRIDE_FIND_PACKAGE
  SYSTEM
  GIT_REPOSITORY https://chromium.googlesource.com/webm/libwebp
  GIT_TAG        v1.5.0
)

FetchContent_MakeAvailable(webp)
