# FreeRDP: A Remote Desktop Protocol Implementation
# iOS Client
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

project("ifreerdp" LANGUAGES C)

include(ExternalProject)

get_filename_component(FREERDP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set(DEPS_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps")
file(MAKE_DIRECTORY ${DEPS_INSTALL_DIR})

option(WITH_FFMPEG "enable FFmpeg (H.264) support" ON)

set(IFREERDP_DEPS "")

include(ExternalOpenSSL)
list(APPEND IFREERDP_DEPS openssl)

if(WITH_FFMPEG)
  include(ExternalFFmpeg)
  list(APPEND IFREERDP_DEPS ffmpeg)
endif()

get_filename_component(_toolchain_abs "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")

# arguments shared by the FreeRDP library build and the iFreeRDP app build
set(_ios_common_cmake_args
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${_toolchain_abs} -DPLATFORM:STRING=${PLATFORM}
    -DDEPLOYMENT_TARGET:STRING=${DEPLOYMENT_TARGET} -DIOS_TARGET_SDK:STRING=${DEPLOYMENT_TARGET}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} -DCMAKE_C_STANDARD:STRING=23
)

set(_ios_ffmpeg_args "")
if(WITH_FFMPEG)
  set(_ios_ffmpeg_args
      -DAVCODEC_INCLUDE_DIRS:PATH=${DEPS_INSTALL_DIR}/FFmpeg.framework/Headers
      -DAVUTIL_INCLUDE_DIRS:PATH=${DEPS_INSTALL_DIR}/FFmpeg.framework/Headers
      -DAVCODEC_LIBRARIES:FILEPATH=${DEPS_INSTALL_DIR}/FFmpeg.framework/FFmpeg
      -DAVUTIL_LIBRARIES:FILEPATH=${DEPS_INSTALL_DIR}/FFmpeg.framework/FFmpeg
  )
endif()

# FreeRDP itself - DEPENDS on everything above.
include(ExternalFreeRDP)

# iFreeRDP application - DEPENDS on the FreeRDP libraries built above and
# links against them with find_package() at the app's configure time.
set(IFREERDP_APP_CMAKE_ARGS ${_ios_common_cmake_args} -DCMAKE_PREFIX_PATH:PATH=${DEPS_INSTALL_DIR}
                            -DFREERDP_IOS_EXTERNAL_SSL_PATH:PATH=${DEPS_INSTALL_DIR} ${_ios_ffmpeg_args}
)

if(FREERDP_IOS_DEVELOPMENT_TEAM)
  list(APPEND IFREERDP_APP_CMAKE_ARGS -DFREERDP_IOS_DEVELOPMENT_TEAM:STRING=${FREERDP_IOS_DEVELOPMENT_TEAM})
endif()

ExternalProject_Add(
  ifreerdp PREFIX ${CMAKE_BINARY_DIR}/ifreerdp SOURCE_DIR ${FREERDP_SOURCE_DIR}/client/iOS CMAKE_GENERATOR "Xcode"
  CMAKE_ARGS ${IFREERDP_APP_CMAKE_ARGS} DEPENDS freerdp ${IFREERDP_DEPS} BUILD_ALWAYS 1 INSTALL_COMMAND ""
)
