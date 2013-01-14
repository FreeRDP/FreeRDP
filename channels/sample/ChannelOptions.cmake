
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT OFF)

if(WITH_SAMPLE)
	set(OPTION_CLIENT_DEFAULT ON)
	set(OPTION_SERVER_DEFAULT OFF)
endif()

define_channel_options(NAME "sample" TYPE "static"
	DESCRIPTION "Sample Virtual Channel Extension"
	SPECIFICATIONS ""
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

