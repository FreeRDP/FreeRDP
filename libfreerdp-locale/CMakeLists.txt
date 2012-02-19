# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-locale cmake build script
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

set(FREERDP_LOCALE_SRCS
	locale.c
	layouts.c
	keyboard.c
	keyboard.h
	keyboard_x11.c
	keyboard_x11.h
	liblocale.h)

set(FREERDP_LOCALE_XKB_SRCS
	keyboard_xkb.c
	keyboard_xkb.h)

if(NOT WIN32)
	find_suggested_package(X11)
	if(NOT APPLE)
		find_suggested_package(XKBFile)
		if (WITH_X11)
			add_definitions(-DWITH_X11)
			if(WITH_XKBFILE)
				add_definitions(-DWITH_XKB)
				include_directories(${XKBFILE_INCLUDE_DIRS})
				set(FREERDP_LOCALE_SRCS ${FREERDP_LOCALE_SRCS} ${FREERDP_LOCALE_XKB_SRCS})
			endif()
		endif()
	endif()
endif()

add_library(freerdp-locale ${FREERDP_LOCALE_SRCS})

if(NOT WIN32)
	if(WITH_X11)
		target_link_libraries(freerdp-locale ${X11_LIBRARIES})
		if(WITH_XKBFILE)
			target_link_libraries(freerdp-locale ${XKBFILE_LIBRARIES})
		endif()
	endif()
endif()

set_target_properties(freerdp-locale PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(freerdp-locale freerdp-utils)

install(TARGETS freerdp-locale DESTINATION ${CMAKE_INSTALL_LIBDIR})

