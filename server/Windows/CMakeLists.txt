# FreeRDP: A Remote Desktop Protocol Client
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

if(WITH_SERVER_INTERFACE)
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")
else()
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} cli/wfreerdp.c cli/wfreerdp.h)
	add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()

if(WITH_WIN8)		
	set(${MODULE_PREFIX}_LIBS d3d11 dxgi dxguid)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp)
else()

	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS}
		freerdp-core
		freerdp-utils
		freerdp-codec
		freerdp-channels)
endif()

target_link_libraries(wfreerdp-server ${${MODULE_PREFIX}_LIBS})


if(WITH_SERVER_INTERFACE)
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(WITH_SERVER_INTERFACE)
	add_subdirectory(cli)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Windows")
