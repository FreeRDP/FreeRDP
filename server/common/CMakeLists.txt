# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Server Common
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

set(MODULE_NAME "freerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER")

#  Policy CMP0022: INTERFACE_LINK_LIBRARIES defines the link
#  interface.  Run "cmake --help-policy CMP0022" for policy details.  Use the
#  cmake_policy command to set the policy and suppress this warning.
if(POLICY CMP0022)
  cmake_policy(SET CMP0022 NEW)
endif()

set(${MODULE_PREFIX}_SRCS
	server.c)

foreach(FREERDP_CHANNELS_SERVER_SRC ${FREERDP_CHANNELS_SERVER_SRCS})
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} "${FREERDP_CHANNELS_SERVER_SRC}")
endforeach()

if(MSVC)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} module.def)
endif()


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

  set (${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if (WITH_LIBRARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} ${FREERDP_CHANNELS_SERVER_LIBS})
target_link_libraries(${MODULE_NAME} ${PUBLIC_KEYWORD} winpr freerdp)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT FreeRDP-ServerTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Common")
export_complex_library(LIBNAME ${MODULE_NAME})
