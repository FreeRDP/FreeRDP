# libfreerdp-gdi

include_directories(.)
include_directories(../include)

set(FREERDP_GDI_SRCS
	color.c
	color.h
	decode.c
	decode.h
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
	gdi.c
	gdi.h)

add_library(freerdp-gdi SHARED ${FREERDP_GDI_SRCS})

set_target_properties(freerdp-gdi PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION})

install(TARGETS freerdp-gdi DESTINATION lib)

