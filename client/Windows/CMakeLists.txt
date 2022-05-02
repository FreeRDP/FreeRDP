# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Windows cmake build script
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

set(MODULE_NAME "wfreerdp-client")
set(MODULE_PREFIX "FREERDP_CLIENT_WINDOWS_CONTROL")

set(${MODULE_PREFIX}_SRCS
	wf_gdi.c
	wf_gdi.h
	wf_event.c
	wf_event.h
	wf_channels.c
	wf_channels.h
	wf_graphics.c
	wf_graphics.h
	wf_cliprdr.c
	wf_cliprdr.h
	wf_rail.c
	wf_rail.h
	wf_client.c
	wf_client.h
	wf_floatbar.c
	wf_floatbar.h
	resource/wfreerdp.rc
	resource/resource.h)

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32 AND BUILD_SHARED_LIBS)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  if(WITH_CLIENT_INTERFACE)
    set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${FREERDP_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
  else()
    set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )
  endif()

  configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

option(WITH_WINDOWS_CERT_STORE "Build ${MODULE_NAME} with additional certificate validation against windows certificate store" ON)
if(WITH_WINDOWS_CERT_STORE)
  add_definitions("-DWITH_WINDOWS_CERT_STORE")
endif()

option(WITH_WIN_CONSOLE "Build ${MODULE_NAME} with console support" OFF)
if(WITH_WIN_CONSOLE)
  add_definitions("-DWITH_WIN_CONSOLE")
  set(WIN32_GUI_FLAG "")
else()
  set(WIN32_GUI_FLAG "WIN32")
endif()

option(WITH_PROGRESS_BAR "Build ${MODULE_NAME} with connect progress bar (Windows 7+ or 2008 R2+)" ON)
if(WITH_PROGRESS_BAR)
  add_definitions("-DWITH_PROGRESS_BAR")
endif()

if(WITH_CLIENT_INTERFACE)
	if(CLIENT_INTERFACE_SHARED)
		add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})
	else()
		add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
	endif()
	if (WITH_LIBRARY_VERSIONING)
        set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
    endif()
    set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${FREERDP_API_VERSION})
else()
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} cli/wfreerdp.c cli/wfreerdp.h)
	add_executable(${MODULE_NAME} ${WIN32_GUI_FLAG} ${${MODULE_PREFIX}_SRCS})
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "wfreerdp")
endif()

list(APPEND LIBS freerdp-client)
list(APPEND LIBS winpr freerdp)
list(APPEND LIBS msimg32 credui ntdll)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

if(WITH_CLIENT_INTERFACE)
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
	if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
		install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
	endif()
	add_subdirectory(cli)
else()
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
	if (WITH_DEBUG_SYMBOLS AND MSVC)
		get_target_property(OUTPUT_FILENAME ${MODULE_NAME} OUTPUT_NAME)
		install(FILES ${CMAKE_PDB_BINARY_DIR}/${OUTPUT_FILENAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
	endif()
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Windows")
