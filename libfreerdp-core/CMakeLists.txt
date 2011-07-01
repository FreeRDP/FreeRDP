# libfreerdp-core

include_directories(.)
include_directories(../include)

add_definitions(-DL_ENDIAN=1)
add_definitions(-DEXT_PATH="/usr/lib/freerdp/extensions")

set(LIBFREERDP_CORE_SRCS
	nego.c
	nego.h
	credssp.c
	credssp.h
	ntlmssp.c
	ntlmssp.h
)

add_library(freerdp-core SHARED ${LIBFREERDP_CORE_SRCS})

install(TARGETS freerdp-core DESTINATION lib)
