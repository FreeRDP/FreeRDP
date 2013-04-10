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
	wf_graphics.c
	wf_graphics.h
	wf_cliprdr.c
	wf_cliprdr.h
	wf_window.c
	wf_window.h
	wf_rail.c
	wf_rail.h
	wf_interface.c
	wf_interface.h
	wfreerdp.rc
	resource.h)

if(WITH_CLIENT_INTERFACE)
	if(CLIENT_INTERFACE_SHARED)
		add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})
	else()
		add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
	endif()
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION} PREFIX "lib")
else()
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} cli/wfreerdp.c cli/wfreerdp.h)
	add_executable(${MODULE_NAME} WIN32 ${${MODULE_PREFIX}_SRCS})
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-crt winpr-credui winpr-utils)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-gdi freerdp-codec freerdp-primitives freerdp-utils)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if(WITH_CLIENT_INTERFACE)
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
	add_subdirectory(cli)
else()
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Windows")
