# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP X11 Client
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

include(FindXmlto)
include_directories(${X11_INCLUDE_DIRS})

add_executable(xfreerdp
	xf_gdi.c
	xf_gdi.h
	xf_rail.c
	xf_rail.h
	xf_tsmf.c
	xf_tsmf.h
	xf_event.c
	xf_event.h
	xf_cliprdr.c
	xf_cliprdr.h
	xf_monitor.c
	xf_monitor.h
	xf_graphics.c
	xf_graphics.h
	xf_keyboard.c
	xf_keyboard.h
	xf_window.c
	xf_window.h
	xfreerdp.c
	xfreerdp.h)

if(WITH_MANPAGES)
	if(XMLTO_FOUND)
		add_custom_command(OUTPUT xfreerdp.1
			COMMAND ${XMLTO_EXECUTABLE} man ${CMAKE_CURRENT_SOURCE_DIR}/xfreerdp.1.xml
			DEPENDS xfreerdp.1.xml)

		add_custom_target(xfreerdp.manpage ALL
			DEPENDS xfreerdp.1)

		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xfreerdp.1 DESTINATION share/man/man1)
	else(XMLTO_FOUND)
		message(WARNING "WITH_MANPAGES was set, but xmlto was not found. man-pages will not be installed")
	endif(XMLTO_FOUND)
endif(WITH_MANPAGES)

find_suggested_package(Xinerama)
if(WITH_XINERAMA)
	add_definitions(-DWITH_XINERAMA -DWITH_XEXT)
	include_directories(${XINERAMA_INCLUDE_DIRS})
	target_link_libraries(xfreerdp ${XINERAMA_LIBRARIES})
endif()

find_suggested_package(Xext)
if(WITH_XEXT)
	add_definitions(-DWITH_XEXT)
	include_directories(${XEXT_INCLUDE_DIRS})
	target_link_libraries(xfreerdp ${XEXT_LIBRARIES})
endif()

find_suggested_package(Xcursor)
if(WITH_XCURSOR)
	add_definitions(-DWITH_XCURSOR)
	include_directories(${XCURSOR_INCLUDE_DIRS})
	target_link_libraries(xfreerdp ${XCURSOR_LIBRARIES})
endif()

find_suggested_package(Xv)
if(WITH_XV)
	add_definitions(-DWITH_XV)
	include_directories(${XV_INCLUDE_DIRS})
	target_link_libraries(xfreerdp ${XV_LIBRARIES})
endif()

include_directories(${CMAKE_SOURCE_DIR}/resources)

target_link_libraries(xfreerdp freerdp-core)
target_link_libraries(xfreerdp freerdp-gdi)
target_link_libraries(xfreerdp freerdp-kbd)
target_link_libraries(xfreerdp freerdp-rail)
target_link_libraries(xfreerdp freerdp-channels)
target_link_libraries(xfreerdp freerdp-utils)
target_link_libraries(xfreerdp ${X11_LIBRARIES} ${CMAKE_DL_LIBS})

install(TARGETS xfreerdp DESTINATION ${CMAKE_INSTALL_BINDIR})
