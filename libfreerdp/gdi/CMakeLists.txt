# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-gdi cmake build script
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(FREERDP_GDI_SRCS
	8bpp.c
	16bpp.c
	32bpp.c
	bitmap.c
	brush.c
	clipping.c
	dc.c
	drawing.c
	line.c
	palette.c
	pen.c
	region.c
	shape.c
	graphics.c
	graphics.h
	gdi.c
	gdi.h)

if(WITH_MONOLITHIC_BUILD)
	add_library(freerdp-gdi OBJECT ${FREERDP_GDI_SRCS})
else()
	add_library(freerdp-gdi ${FREERDP_GDI_SRCS})
endif()

set_target_properties(freerdp-gdi PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(FREERDP_GDI_LIBS)

if(WITH_MONOLITHIC_BUILD)
	
else()
	set(FREERDP_GDI_LIBS ${FREERDP_GDI_LIBS}
		freerdp-core
		freerdp-cache
		freerdp-codec)
	
	target_link_libraries(freerdp-gdi ${FREERDP_GDI_LIBS})
	install(TARGETS freerdp-gdi DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

