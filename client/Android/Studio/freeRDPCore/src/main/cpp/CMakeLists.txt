# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Bernhard Miklautz <bernhard.miklautz@thincast.com>
# Copyright 2022 Ely Ronnen <elyronnen@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project("freerdp-android" LANGUAGES C VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(MODULE_NAME "freerdp-android")
set(MODULE_PREFIX "FREERDP_CLIENT_ANDROID")

get_filename_component(FREERDP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../.." ABSOLUTE)

include(GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR ".")

# FREERDP_EXTERNAL_PATH points here; FreeRDP appends /${ANDROID_ABI}/ internally
set(DEPS_EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs")
# All external deps install into this ABI-scoped prefix
set(DEPS_INSTALL_DIR "${DEPS_EXTERNAL_ROOT}/${CMAKE_ANDROID_ARCH_ABI}")

# Module path for our cmake helpers
list(APPEND CMAKE_MODULE_PATH "${FREERDP_SOURCE_DIR}/client/Android/cmake")

include(AndroidHelpers)
detect_ndk_root(NDK_ROOT)
detect_ndk_host_platform(NDK_HOST_PLATFORM)
get_android_api_level(NDK_API_LEVEL)

option(WITH_OPENSSL "Build and enable OpenSSL" ON)
option(WITH_FFMPEG "Build and enable FFmpeg codec support" ON)
option(WITH_OPENH264 "Build and enable OpenH264 codec support" ON)
option(WITH_CJSON "Build cJSON for Azure AD (AAD) support" ON)
option(WITH_MEDIACODEC "Enable Android MediaCodec API" OFF)
option(WITH_OPUS "Build Opus audio codec" ON)
option(WITH_PNG "Build libpng image support" ON)
option(WITH_WEBP "Build libwebp image support" ON)
option(WITH_JPEG "Build libjpeg-turbo image support" ON)

set(ANDROID_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
    -DANDROID_ABI:STRING=${CMAKE_ANDROID_ARCH_ABI}
    -DANDROID_PLATFORM:STRING=android-${NDK_API_LEVEL}
    -DANDROID_STL:STRING=c++_shared
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
    -DBUILD_SHARED_LIBS:BOOL=ON
)

set(ANDROID_NATIVE_DEPS "")

if(WITH_OPENSSL)
  include(ExternalOpenSSL)
  list(APPEND ANDROID_NATIVE_DEPS openssl)
endif()

if(WITH_CJSON)
  include(ExternalCJSON)
  list(APPEND ANDROID_NATIVE_DEPS cjson)
endif()

if(WITH_OPENH264)
  include(ExternalOpenH264)
  list(APPEND ANDROID_NATIVE_DEPS openh264)
endif()

if(WITH_OPUS)
  include(ExternalOpus)
  list(APPEND ANDROID_NATIVE_DEPS opus)
endif()

if(WITH_FFMPEG)
  include(ExternalFFmpeg)
  list(APPEND ANDROID_NATIVE_DEPS ffmpeg)
endif()

if(WITH_PNG)
  include(ExternalLibPNG)
  list(APPEND ANDROID_NATIVE_DEPS libpng)
endif()

if(WITH_WEBP)
  include(ExternalWebP)
  list(APPEND ANDROID_NATIVE_DEPS webp)
endif()

if(WITH_JPEG)
  include(ExternalJpeg)
  list(APPEND ANDROID_NATIVE_DEPS jpeg)
endif()

include(ExternalUriparser)
list(APPEND ANDROID_NATIVE_DEPS uriparser)

# FreeRDP itself — DEPENDS on everything above.
# Also defines IMPORTED targets: freerdp3-lib, freerdp-client3-lib, winpr3-lib
include(ExternalFreeRDP)

set(${MODULE_PREFIX}_SRCS
    android_cliprdr.c
    android_cliprdr.h
    android_disp.c
    android_disp.h
    android_event.c
    android_event.h
    android_freerdp.c
    android_freerdp.h
    android_jni_utils.c
    android_jni_utils.h
    android_jni_callback.c
    android_jni_callback.h
)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

add_dependencies(${MODULE_NAME} freerdp)

find_library(log-lib log)
find_library(dl-lib dl)
find_library(jnigraphics-lib jnigraphics)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(
  ${MODULE_NAME}
  freerdp3-lib
  freerdp-client3-lib
  winpr3-lib
  ${log-lib}
  ${dl-lib}
  ${jnigraphics-lib}
)
