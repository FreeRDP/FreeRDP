
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT ON)

if(ANDROID)
	set(OPTION_SERVER_DEFAULT OFF)
endif()

define_channel_options(NAME "audin" TYPE "dynamic"
	DESCRIPTION "Audio Input Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEAI]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

