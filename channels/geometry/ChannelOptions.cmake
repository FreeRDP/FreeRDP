
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(NAME "geometry" TYPE "dynamic"
	DESCRIPTION "Geometry tracking Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEGT]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
