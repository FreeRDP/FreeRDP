
set(MODULE_NAME pf_channels)
set(SOURCES
	pf_channel_rdpdr.c
	pf_channel_rdpdr.h
)

if (WITH_PROXY_EMULATE_SMARTCARD)
	list(APPEND SOURCES
		pf_channel_smartcard.c
		pf_channel_smartcard.h
	)
endif()

add_library(${MODULE_NAME} OBJECT ${SOURCES})
