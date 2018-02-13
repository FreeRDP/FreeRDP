
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(NAME "video" TYPE "dynamic"
	DESCRIPTION "Video optimized remoting Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEVOR]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})

