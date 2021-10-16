if(WITH_CHANNEL_GFXREDIR)
	set(OPTION_DEFAULT OFF)
	set(OPTION_CLIENT_DEFAULT OFF)
	set(OPTION_SERVER_DEFAULT ON)

	define_channel_options(NAME "gfxredir" TYPE "dynamic"
		DESCRIPTION "Graphics Redirection Virtual Channel Extension"
		SPECIFICATIONS "[MS-RDPXXXX]"
		DEFAULT ${OPTION_DEFAULT})

	define_channel_server_options(${OPTION_SERVER_DEFAULT})
endif()
