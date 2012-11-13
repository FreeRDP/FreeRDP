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

set(MODULE_NAME "wfreerdp")
set(MODULE_PREFIX "FREERDP_CLIENT_WINDOWS")

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
	wfreerdp.c
	wfreerdp.h)

add_executable(${MODULE_NAME} WIN32 ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-gdi freerdp-codec freerdp-utils)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Windows")
