# FreeRDP: A Remote Desktop Protocol Implementation
# MS-RDPECAM AVFoundation camera HAL for macOS and iOS
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

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_avfoundation_minimum_version "10.15")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_avfoundation_minimum_version "10.0")
else()
  message(FATAL_ERROR "AVFoundation camera redirection requires a macOS or iOS target")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS _avfoundation_minimum_version)
  message(
    FATAL_ERROR
      "AVFoundation camera redirection requires ${CMAKE_SYSTEM_NAME} ${_avfoundation_minimum_version} or later"
  )
endif()

define_channel_client_subsystem("rdpecam" "avfoundation" "")

# These cached framework paths are shared with the native VideoToolbox codec.
# Resolve them here too so camera capture works with the H.264 decoder disabled.
find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
find_library(COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
find_library(VIDEOTOOLBOX_FRAMEWORK VideoToolbox REQUIRED)

set(${MODULE_PREFIX}_SRCS camera_avfoundation.m capture_policy.h frame_rate.h modes.h modes.m nv12.c)
set(${MODULE_PREFIX}_LIBS winpr freerdp ${AVFOUNDATION_FRAMEWORK} ${COREMEDIA_FRAMEWORK} ${COREVIDEO_FRAMEWORK}
                          ${VIDEOTOOLBOX_FRAMEWORK}
)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")
# Keep manual retain/release even when the iOS toolchain or parent project enables ARC.
set_source_files_properties(camera_avfoundation.m modes.m PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_subdirectory(test)
endif()
