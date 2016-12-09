# WinPR: Windows Portable Runtime
# winpr-makecert cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2016 Thincast Technologies GmbH
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

set(MODULE_NAME "winpr-makecert")
set(MODULE_PREFIX "WINPR_MAKECERT")

set(${MODULE_PREFIX}_SRCS
	main.c)

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
	set(RC_VERSION_MAJOR ${WINPR_VERSION_MAJOR})
	set(RC_VERSION_MINOR ${WINPR_VERSION_MINOR})
	set(RC_VERSION_BUILD ${WINPR_VERSION_REVISION})
	set(RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")

	configure_file(
		${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/version.rc
		@ONLY)

	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS winpr-tools)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools EXPORT WinPRTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
endif()
