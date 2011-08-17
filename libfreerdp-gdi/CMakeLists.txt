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

set(FREERDP_GDI_SRCS
	color.c
	gdi_8bpp.c
	gdi_8bpp.h
	gdi_16bpp.c
	gdi_16bpp.h
	gdi_32bpp.c
	gdi_32bpp.h
	gdi_bitmap.c
	gdi_bitmap.h
	gdi_brush.c
	gdi_brush.h
	gdi_clipping.c
	gdi_clipping.h
	gdi_dc.c
	gdi_dc.h
	gdi_drawing.c
	gdi_drawing.h
	gdi_line.c
	gdi_line.h
	gdi_palette.c
	gdi_palette.h
	gdi_pen.c
	gdi_pen.h
	gdi_region.c
	gdi_region.h
	gdi_shape.c
	gdi_shape.h
	gdi.c)

add_library(freerdp-gdi SHARED ${FREERDP_GDI_SRCS})

target_link_libraries(freerdp-gdi freerdp-cache)
target_link_libraries(freerdp-gdi freerdp-rfx)

set_target_properties(freerdp-gdi PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

install(TARGETS freerdp-gdi DESTINATION lib)
