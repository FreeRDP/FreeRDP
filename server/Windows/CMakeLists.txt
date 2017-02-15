# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Windows Server cmake build script
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

set(MODULE_NAME "wfreerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER_WINDOWS")

include_directories(.)

set(${MODULE_PREFIX}_SRCS
	wf_update.c
	wf_update.h
	wf_dxgi.c
	wf_dxgi.h
	wf_input.c
	wf_input.h
	wf_interface.c
	wf_interface.h
	wf_mirage.c
	wf_mirage.h
	wf_peer.c
	wf_peer.h
	wf_settings.c
	wf_settings.h
	wf_info.c
	wf_info.h)
	
if(CHANNEL_RDPSND AND NOT WITH_RDPSND_DSOUND)
	set(${MODULE_PREFIX}_SRCS
		${${MODULE_PREFIX}_SRCS}
		wf_rdpsnd.c
		wf_rdpsnd.h
		wf_wasapi.c
		wf_wasapi.h
		)
endif()

if(CHANNEL_RDPSND AND WITH_RDPSND_DSOUND)
	set(${MODULE_PREFIX}_SRCS
		${${MODULE_PREFIX}_SRCS}
		wf_rdpsnd.c
		wf_rdpsnd.h
		wf_directsound.c
		wf_directsound.h
		)
endif()

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
if(WITH_SERVER_INTERFACE)
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
else()
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

set (${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

if(WITH_SERVER_INTERFACE)
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
  if (WITH_LIBRARY_VERSIONING)
  	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
  endif()
  
else()
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} cli/wfreerdp.c cli/wfreerdp.h)
	add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()


if(CMAKE_WINDOWS_VERSION STREQUAL "WIN8")
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} d3d11 dxgi dxguid)
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} dsound)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-server freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if(WITH_SERVER_INTERFACE)
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)

	if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
		install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
	endif()
else()
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)

	if (WITH_DEBUG_SYMBOLS AND MSVC)
		install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
	endif()
endif()

if(WITH_SERVER_INTERFACE)
	add_subdirectory(cli)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Windows")
