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

include_directories(.)

set(WFREERDP_SERVER_SRCS
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
	add_library(wfreerdp-server ${WFREERDP_SERVER_SRCS})
	set_target_properties(wfreerdp-server PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")
else()
	set(WFREERDP_SERVER_SRCS ${WFREERDP_SERVER_SRCS} cli/wfreerdp.c cli/wfreerdp.h)
	add_executable(wfreerdp-server ${WFREERDP_SERVER_SRCS})
endif()

if(WITH_WIN8)		
	set(WFREERDP_SERVER_LIBS d3d11 dxgi dxguid)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(WFREERDP_SERVER_LIBS ${WFREERDP_SERVER_LIBS} freerdp)
else()

	set(WFREERDP_SERVER_LIBS ${WFREERDP_SERVER_LIBS}
		freerdp-core
		freerdp-utils
		freerdp-codec
		freerdp-channels)
endif()

target_link_libraries(wfreerdp-server ${WFREERDP_SERVER_LIBS})


if(WITH_SERVER_INTERFACE)
	install(TARGETS wfreerdp-server DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
	install(TARGETS wfreerdp-server DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(WITH_SERVER_INTERFACE)
	add_subdirectory(cli)
endif()
