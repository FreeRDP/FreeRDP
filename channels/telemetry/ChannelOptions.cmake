
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT ON)

define_channel_options(NAME "telemetry" TYPE "dynamic"
	DESCRIPTION "Telemetry Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPET]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_server_options(${OPTION_SERVER_DEFAULT})

