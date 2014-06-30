# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2013 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

if (NOT ANDROID_NDK)
	message(FATAL_ERROR "ANDROID_NDK not set but required for building android native library.")
endif()

set(CMAKE_PROGRAM_PATH ${ANDROID_NDK})
find_program(NDK_COMMAND ndk-build CMAKE_FIND_ROOT_PATH_BOTH)

if(NDK_COMMAND STREQUAL "NDK_COMMAND-NOTFOUND")
	message(FATAL_ERROR "ndk-build not found but required to build native lib")
endif()

set(NDK_LIB_CFG "${CMAKE_CURRENT_BINARY_DIR}/FreeRDPCore/jni/Android.mk")
if(ANDROID_BUILD_JAVA)
	if (NOT ANDROID_SDK)
		message(FATAL_ERROR "ANDROID_SDK not set but required for building the java gui (ANDROID_BUILD_JAVA)")
	endif()
	# And isn't shiped with the android ndk/sdk so
	# we need to find it on the local machine
  SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
	find_program(ANT_COMMAND ant)
  SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)

	if(ANT_COMMAND STREQUAL "ANT_COMMAND-NOTFOUND")
		message(FATAL_ERROR "ant not found but required to build android java")
	endif()
endif(ANDROID_BUILD_JAVA)

set(ANDROID_COMMAND "${ANDROID_SDK}/tools/android")
if(NOT EXISTS ${ANDROID_COMMAND})
	message(FATAL_ERROR "android not found but required to build android java")
endif()

if(ANDROID_BUILD_JAVA_DEBUG)
	set(ANDROID_BUILD_TYPE "debug")
else()
	set(ANDROID_BUILD_TYPE "release")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(ANDROID_DEBUG_ENABLE "true")
	set(NDK_DEBUG "1")
else()
	set(ANDROID_DEBUG_ENABLE "false")
	set(NDK_DEBUG "0")
endif()

set(APPCOMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/appcompat_v7")
add_custom_target(copy_appcompat ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${ANDROID_SDK}/extras/android/support/v7/appcompat ${APPCOMPAT_DIR}
  COMMAND ${ANDROID_COMMAND} update lib-project -p ${APPCOMPAT_DIR}
)

add_subdirectory(FreeRDPCore)
add_subdirectory(aFreeRDP)
