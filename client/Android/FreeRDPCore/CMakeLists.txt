# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(ANDROID_PACKAGE_NAME "aFreeRDPCore")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/AndroidManifest.xml.cmake
	${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build.xml.cmake
	${CMAKE_CURRENT_BINARY_DIR}/build.xml @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/project.properties.cmake
	${CMAKE_CURRENT_BINARY_DIR}/project.properties @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/ant.properties.cmake
	${CMAKE_CURRENT_BINARY_DIR}/ant.properties @ONLY)

# Generate a Java class with static members set to the CMake
# configuration properties.
set(JAVA_CFG "src/com/freerdp/freerdpcore/utils/BuildConfiguration.java")
set(JAVA_CFG_OUT "${CMAKE_CURRENT_BINARY_DIR}/${JAVA_CFG}")
set(JAVA_CFG_IN "${CMAKE_CURRENT_SOURCE_DIR}/${JAVA_CFG}.in")

CONFIGURE_FILE(${JAVA_CFG_IN} ${JAVA_CFG_OUT})

file(COPY res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (ANDROID_SDK)
	CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/local.properties.cmake
		${CMAKE_CURRENT_BINARY_DIR}/local.properties @ONLY)
endif()

add_subdirectory(jni)

SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "gen;bin;obj;libs")

