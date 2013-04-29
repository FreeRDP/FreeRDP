
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(NAME "drive" TYPE "device"
	DESCRIPTION "Drive Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEFS]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

