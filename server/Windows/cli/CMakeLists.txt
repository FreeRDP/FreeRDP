# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Windows Server (CLI) cmake build script
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

set(MODULE_NAME "wfreerdp-server-cli")
set(MODULE_PREFIX "FREERDP_SERVER_WINDOWS_CLI")

include_directories(..)

set(${MODULE_PREFIX}_SRCS
	wfreerdp.c
	wfreerdp.h)

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

set (${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "wfreerdp-server")

set(${MODULE_PREFIX}_LIBS wfreerdp-server)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)

if (WITH_DEBUG_SYMBOLS AND MSVC)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Windows")
