# WinPR: Windows Portable Runtime
# winpr-makecert cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(MODULE_NAME "winpr-makecert-tool")
set(MODULE_VERSIONED_NAME "winpr${WINPR_VERSION_MAJOR}-makecert-tool")
set(MODULE_PREFIX "WINPR_MAKECERT_TOOL")

set(${MODULE_PREFIX}_SRCS makecert.c)

if(OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if(MBEDTLS_FOUND)
	include_directories(${MBEDTLS_INCLUDE_DIR})
endif()

if (WIN32)
	set(RC_VERSION_MAJOR ${WINPR_VERSION_MAJOR})
	set(RC_VERSION_MINOR ${WINPR_VERSION_MINOR})
	set(RC_VERSION_BUILD ${WINPR_VERSION_REVISION})
	set(RC_VERSION_FILE "${MODULE_VERSIONED_NAME}${CMAKE_LIBRARY_SUFFIX}")

	configure_file(
		${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/version.rc
		@ONLY)

	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if (WITH_LIBRARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

if (WIN32)
	set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME
			${MODULE_VERSIONED_NAME})
endif (WIN32)

set(${MODULE_PREFIX}_LIBS winpr)

if(OPENSSL_FOUND)
	list(APPEND ${MODULE_PREFIX}_LIBS ${OPENSSL_LIBRARIES})
endif()

if(MBEDTLS_FOUND)
	list(APPEND ${MODULE_PREFIX}_LIBS ${MBEDTLS_LIBRARIES})
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT WinPRTargets)

add_subdirectory(cli)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")
export_complex_library(LIBNAME ${MODULE_NAME})
