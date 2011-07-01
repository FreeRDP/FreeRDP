# libfreerdp-kbd

include_directories(.)
include_directories(../include)

set(FREERDP_KBD_SRCS
	keyboard.h
	locales.c
	locales.h
	layout_ids.c
	layout_ids.h
	layouts_xkb.c
	layouts_xkb.h
	x_layout_id_table.c
	x_layout_id_table.h
	libkbd.c
	libkbd.h)

add_definitions(-DKEYMAP_PATH="${FREERDP_KEYMAP_PATH}")

add_library(freerdp-kbd SHARED ${FREERDP_KBD_SRCS})

set_target_properties(freerdp-kbd PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION})

install(TARGETS freerdp-kbd DESTINATION lib)

