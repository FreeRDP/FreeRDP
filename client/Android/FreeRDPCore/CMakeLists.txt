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

set(ANDROID_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ANDROID_PACKAGE_NAME "aFreeRDPCore")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/AndroidManifest.xml.cmake ${CMAKE_CURRENT_SOURCE_DIR}/AndroidManifest.xml @ONLY)

if (ANDROID_SDK)
	CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/local.properties.cmake ${CMAKE_CURRENT_SOURCE_DIR}/local.properties @ONLY)
endif()

add_subdirectory(jni)


if(ANDROID_BUILD_JAVA)
	set(ANDROIDLIB "${ANDROID_SOURCE_DIR}/bin/classes.jar")

  # command to create the android package
	add_custom_command(
		OUTPUT "${ANDROIDLIB}"
		COMMAND ${ANT_COMMAND} ${ANDROID_BUILD_TYPE}
		WORKING_DIRECTORY "${ANDROID_SOURCE_DIR}"
		MAIN_DEPENDENCY AndroidManifest.xml
		DEPENDS freerdp-android local.properties
	)
	add_custom_target(android-lib ALL SOURCES "${ANDROIDLIB}")
	SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "gen;bin")
endif()
