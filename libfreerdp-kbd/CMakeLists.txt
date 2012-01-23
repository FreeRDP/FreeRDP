# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-kbd cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(FREERDP_KBD_SRCS
	locales.c
	layouts.c
	layouts_xkb.c
	layouts_xkb.h
	x_layout_id_table.c
	x_layout_id_table.h
	libkbd.c
	libkbd.h)

add_library(freerdp-kbd ${FREERDP_KBD_SRCS})

find_suggested_package(X11)
if(WITH_X11)
	target_link_libraries(freerdp-kbd ${X11_LIBRARIES})
endif()

if(NOT APPLE)
	find_suggested_package(XKBFile)
	if(WITH_X11)
		if(WITH_XKBFILE)
			add_definitions(-DWITH_XKBFILE)
			include_directories(${XKBFILE_INCLUDE_DIRS})
			target_link_libraries(freerdp-kbd ${XKBFILE_LIBRARIES})
		endif()
	endif()
endif()

add_definitions(-DKEYMAP_PATH="${FREERDP_KEYMAP_PATH}")

set_target_properties(freerdp-kbd PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

install(TARGETS freerdp-kbd DESTINATION ${CMAKE_INSTALL_LIBDIR})

