include(GNUInstallDirs)

set(DEPS_EXTERNAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../Studio/freeRDPCore/src/main/jniLibs")

# All external deps install into this ABI-scoped prefix
set(DEPS_INSTALL_DIR "${DEPS_EXTERNAL_ROOT}/${CMAKE_IOS_ARCH_ABI}")

option(WITH_OPENSSL "Build and enable OpenSSL" ON)
option(WITH_FFMPEG "Build and enable FFmpeg codec support" ON)
option(WITH_OPENH264 "Build and enable OpenH264 codec support" ON)
option(WITH_CJSON "Build cJSON for Azure AD (AAD) support" ON)
option(WITH_MEDIACODEC "Enable Android MediaCodec API" OFF)
option(WITH_OPUS "Build Opus audio codec" ON)
option(WITH_PNG "Build libpng image support" ON)
option(WITH_WEBP "Build libwebp image support" ON)
option(WITH_JPEG "Build libjpeg-turbo image support" ON)

set(IOS_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
    -DIOS_ABI:STRING=${CMAKE_IOS_ARCH_ABI}
    -DIOS_PLATFORM:STRING=android-${NDK_API_LEVEL}
    -DIOS_STL:STRING=c++_shared
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
    -DBUILD_SHARED_LIBS:BOOL=ON
)

set(IOS_NATIVE_DEPS "")

if(WITH_OPENSSL)
  include(ExternalOpenSSL)
  list(APPEND IOS_NATIVE_DEPS openssl)
endif()

if(WITH_CJSON)
  include(ExternalCJSON)
  list(APPEND IOS_NATIVE_DEPS cjson)
endif()

if(WITH_OPENH264)
  include(ExternalOpenH264)
  list(APPEND IOS_NATIVE_DEPS openh264)
endif()

if(WITH_OPUS)
  include(ExternalOpus)
  list(APPEND IOS_NATIVE_DEPS opus)
endif()

if(WITH_PNG)
  include(ExternalLibPNG)
  list(APPEND IOS_NATIVE_DEPS libpng)
endif()

if(WITH_WEBP)
  include(ExternalWebP)
  list(APPEND IOS_NATIVE_DEPS webp)
endif()

if(WITH_JPEG)
  include(ExternalJpeg)
  list(APPEND IOS_NATIVE_DEPS jpeg)
endif()

if(WITH_FFMPEG)
  include(ExternalFFmpeg)
  list(APPEND IOS_NATIVE_DEPS ffmpeg)
endif()

include(ExternalUriparser)
list(APPEND IOS_NATIVE_DEPS uriparser)

# FreeRDP itself — DEPENDS on everything above.
# Also defines IMPORTED targets: freerdp3-lib, freerdp-client3-lib, winpr3-lib
include(ExternalFreeRDP)
