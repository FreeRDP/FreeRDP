
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(NAME "rail" TYPE "static"
	DESCRIPTION "Remote Programs Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPERP]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

