include(FetchContent)

set(WITH_SERVER OFF CACHE INTERNAL "fetch content")
set(WITH_PROXY OFF CACHE INTERNAL "fetch content")
set(WITH_RDTK OFF CACHE INTERNAL "fetch content")
set(WITH_SAMPLE OFF CACHE INTERNAL "fetch content")
set(WITH_WINPR_TOOLS OFF CACHE INTERNAL "fetch content")
set(WITH_DOCUMENTATION OFF CACHE INTERNAL "fetch content")
set(WITH_SWSCALE OFF CACHE INTERNAL "fetch content")
set(WITH_FFMPEG OFF CACHE INTERNAL "fetch content")
set(WITH_OPENH264 OFF CACHE INTERNAL "fetch content")

set(WITH_INTERNAL_MD4 ON CACHE INTERNAL "fetch content")
set(WITH_INTERNAL_MD5 ON CACHE INTERNAL "fetch content")
set(WITH_INTERNAL_RC4 ON CACHE INTERNAL "fetch content")
set(WINPR_UTILS_IMAGE_PNG ON CACHE INTERNAL "fetch content")
set(WINPR_UTILS_IMAGE_WEBP ON CACHE INTERNAL "fetch content")
#set(WINPR_UTILS_IMAGE_JPEG ON CACHE INTERNAL "fetch content")
set(WITH_LIBRESSL ON CACHE INTERNAL "fetch content")
set(WITH_CJSON_REQUIRED ON CACHE INTERNAL "fetch content")
set(WITH_ZLIB ON CACHE INTERNAL "fetch content")
set(WITH_OPUS ON CACHE INTERNAL "fetch content")
set(WITH_URIPARSER OFF CACHE INTERNAL "fetch content")

set(WITH_MANPAGES OFF CACHE INTERNAL "fetch content")

include (FetchLibreSSL)
include (FetchcJSON)
#include (ExternalOpenH264)
#include (ExternalFFMPEG)
include (FetchUriParser)
include (FetchOpus)
include (FetchPNG)
include (FetchWebP)
include (ExternalJPEG)
include (FetchLibUSB)

FetchContent_Declare(
  FreeRDP
  OVERRIDE_FIND_PACKAGE
  SYSTEM
  SOURCE_DIR     "${CMAKE_CURRENT_LIST_DIR}/../../../../../../../../"
)

FetchContent_MakeAvailable(FreeRDP)
