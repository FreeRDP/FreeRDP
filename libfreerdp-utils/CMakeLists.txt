# libfreerdp-utils

include_directories(.)
include_directories(../include)

set(FREERDP_UTILS_SRCS
	datablob.c
	hexdump.c
	memory.c
	semaphore.c
	unicode.c)

add_library(freerdp-utils SHARED ${FREERDP_UTILS_SRCS})

set_target_properties(freerdp-utils PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION})

target_link_libraries(freerdp-utils pthread)

install(TARGETS freerdp-utils DESTINATION lib)


