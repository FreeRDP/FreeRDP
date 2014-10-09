# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
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

set(MODULE_NAME "freerdp-channels-server")
set(MODULE_PREFIX "FREERDP_CHANNELS_SERVER")

set(${MODULE_PREFIX}_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/channels.c
	${CMAKE_CURRENT_SOURCE_DIR}/channels.h)
	
foreach(STATIC_MODULE ${CHANNEL_STATIC_SERVER_MODULES})
	set(STATIC_MODULE_NAME ${${STATIC_MODULE}_SERVER_NAME})
	set(STATIC_MODULE_CHANNEL ${${STATIC_MODULE}_SERVER_CHANNEL})
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${STATIC_MODULE_NAME})
endforeach()

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()


add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

if (WITH_LIBRARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "lib")

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} PARENT_SCOPE)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Common")
