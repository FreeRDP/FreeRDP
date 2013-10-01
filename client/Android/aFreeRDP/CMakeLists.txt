# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Bernhard Miklautz <bmiklautz@thinstuff.at>
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

set(ANDROID_PACKAGE_NAME "aFreeRDP")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/AndroidManifest.xml.cmake
	${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build.xml.cmake
	${CMAKE_CURRENT_BINARY_DIR}/build.xml @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/project.properties.cmake
	${CMAKE_CURRENT_BINARY_DIR}/project.properties @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/ant.properties.cmake
	${CMAKE_CURRENT_BINARY_DIR}/ant.properties @ONLY)

file(COPY res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (ANDROID_SDK)
	CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/local.properties.cmake
		${CMAKE_CURRENT_BINARY_DIR}/local.properties @ONLY)
endif()

if("${ANDROID_DEBUG_ENABLE}" STREQUAL "true")
	# 1. generate Android.mk
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/jni/Android.mk
		"APP_ABI := ${ANDROID_NDK_ABI_NAME}\n")

	# 2. generate gdb.setup
	get_directory_property(INCLUDE_DIRECTORIES DIRECTORY . INCLUDE_DIRECTORIES)
	string(REGEX REPLACE ";" " " INCLUDE_DIRECTORIES "${INCLUDE_DIRECTORIES}")
	set(LIB_DIRECTORIES "${ANDROID_NDK}/platforms/android-${ANDROID_NATIVE_API_LEVEL}/arch-${ANDROID_ARCH_NAME}")
	file(WRITE ${ANDROID_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME}/gdb.setup
		"set solib-search-path ${CMAKE_CURRENT_BINARY_DIR}/obj/local/${ANDROID_NDK_ABI_NAME}\n")
	file(APPEND ${ANDROID_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME}/gdb.setup
		"directory ${INCLUDE_DIRECTORIES} ${LIB_DIRECTORIES}\n")

	# 3. copy gdbserver executable
	file(COPY ${ANDROID_NDK}/prebuilt/android-${ANDROID_ARCH_NAME}/gdbserver/gdbserver
		DESTINATION ${ANDROID_BINARY_DIR}/libs/${ANDROID_NDK_ABI_NAME}/)

	# 4. Convenience target to launch debugger.
	add_custom_target(debug-ndk
		COMMAND adb install -r bin/aFreeRDP-debug.apk
		COMMAND ndk-gdb --start --verbose --force
		WORKING_DIRECTORY ${CURRENT_BINARY_DIR}
		)
endif()

if(ANDROID_BUILD_JAVA)
	if(NOT ANDROID_BUILD_JAVA_DEBUG)
		set(APK "${ANDROID_BINARY_DIR}/bin/${ANDROID_PACKAGE_NAME}-release-unsigned.apk")
	else()
		set(APK "${ANDROID_BINARY_DIR}/bin/${ANDROID_PACKAGE_NAME}-debug.apk")
	endif()

  # command to create the android package
	add_custom_command(
		OUTPUT "${APK}"
		COMMAND ${ANT_COMMAND} ${ANDROID_BUILD_TYPE}
		WORKING_DIRECTORY "${ANDROID_BINARY_DIR}"
		MAIN_DEPENDENCY AndroidManifest.xml
		DEPENDS freerdp-android
			${CMAKE_CURRENT_BINARY_DIR}/local.properties
	)
	add_custom_target(android-package ALL SOURCES "${APK}")
	SET_DIRECTORY_PROPERTIES(PROPERTIES  ADDITIONAL_MAKE_CLEAN_FILES "gen;bin")
endif()
