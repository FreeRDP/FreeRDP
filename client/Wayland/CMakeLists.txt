# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Wayland Client cmake build script
#
# Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
# Copyright 2015 David Fort <contact@hardening-consulting.com>
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

set(MODULE_NAME "wlfreerdp")
set(MODULE_PREFIX "FREERDP_CLIENT_WAYLAND")

include_directories(${WAYLAND_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/uwac/include)

set(${MODULE_PREFIX}_SRCS
    wlfreerdp.c
	wlfreerdp.h
	wlf_disp.c
	wlf_disp.h
	wlf_pointer.c
	wlf_pointer.h
	wlf_input.c
	wlf_input.h
	wlf_cliprdr.c
	wlf_cliprdr.h
	wlf_channels.c
	wlf_channels.h
	)

list (APPEND ${MODULE_PREFIX}_LIBS freerdp-client freerdp uwac)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Wayland")
configure_file(wlfreerdp.1.in ${CMAKE_CURRENT_BINARY_DIR}/wlfreerdp.1)
install_freerdp_man(${CMAKE_CURRENT_BINARY_DIR}/wlfreerdp.1 1)
