
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT OFF)

if(WIN32)
	set(OPTION_CLIENT_DEFAULT OFF)
	set(OPTION_SERVER_DEFAULT OFF)
endif()

if(ANDROID)
	set(OPTION_CLIENT_DEFAULT OFF)
	set(OPTION_SERVER_DEFAULT OFF)
endif()

define_channel_options(NAME "tsmf" TYPE "dynamic"
	DESCRIPTION "[DEPRECATED] Video Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEV]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

