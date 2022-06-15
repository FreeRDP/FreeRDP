
set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT ON)

define_channel_options(NAME "camera-device" TYPE "dynamic"
	DESCRIPTION "Video Capture Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPECAM]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_options(NAME "camera-device-enumerator" TYPE "dynamic"
	DESCRIPTION "Video Capture Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPECAM]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_server_options(${OPTION_SERVER_DEFAULT})

