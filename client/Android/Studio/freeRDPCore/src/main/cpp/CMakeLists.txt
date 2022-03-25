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

cmake_minimum_required(VERSION 3.4)

project("freerdp-android")

set(MODULE_NAME "freerdp-android")
set(MODULE_PREFIX "FREERDP_CLIENT_ANDROID")

set(FREERDP_IMPORT_DIR_RELATIVE ../jniLibs/${CMAKE_ANDROID_ARCH_ABI})
get_filename_component(FREERDP_IMPORT_DIR ${FREERDP_IMPORT_DIR_RELATIVE} ABSOLUTE)

include_directories(
        ${FREERDP_IMPORT_DIR}/include/freerdp3
        ${FREERDP_IMPORT_DIR}/include/winpr3
        ${FREERDP_IMPORT_DIR}/include/openssl
)

set(${MODULE_PREFIX}_SRCS
        android_event.c
        android_event.h
        android_freerdp.c
        android_freerdp.h
        android_jni_utils.c
        android_jni_utils.h
        android_jni_callback.c
        android_jni_callback.h)

if(WITH_CLIENT_CHANNELS)
    set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS}
            android_cliprdr.c
            android_cliprdr.h)
endif()

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

add_library(freerdp3-lib SHARED IMPORTED)
set_property(TARGET freerdp3-lib PROPERTY IMPORTED_LOCATION ${FREERDP_IMPORT_DIR}/libfreerdp3.so)

add_library(freerdp-client3-lib SHARED IMPORTED)
set_property(TARGET freerdp-client3-lib PROPERTY IMPORTED_LOCATION ${FREERDP_IMPORT_DIR}/libfreerdp-client3.so)

add_library(winpr3-lib SHARED IMPORTED)
set_property(TARGET winpr3-lib PROPERTY IMPORTED_LOCATION ${FREERDP_IMPORT_DIR}/libwinpr3.so)

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
        ${jnigraphics-lib} )
