
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT ON)

define_channel_options(NAME "encomsp" TYPE "static"
	DESCRIPTION "Multiparty Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEMC]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})

