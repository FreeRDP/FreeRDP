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

# Module path for our cmake helpers
list(APPEND CMAKE_MODULE_PATH "${FREERDP_SOURCE_DIR}/client/Android/cmake")

include(AndroidHelpers)
detect_ndk_root(NDK_ROOT)
detect_ndk_host_platform(NDK_HOST_PLATFORM)
get_android_api_level(NDK_API_LEVEL)

include(ExternalDeps)
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
    android_rail.c
    android_rail.h
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
