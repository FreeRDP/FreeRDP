
set(OPTION_DEFAULT ON)
set(OPTION_SERVER_DEFAULT ON)

set(OPTION_CLIENT_DEFAULT OFF)
if (NOT IOS AND NOT ANDROID AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPTION_CLIENT_DEFAULT ON)
endif()

define_channel_options(NAME "rdpecam" TYPE "dynamic"
	DESCRIPTION "Video Capture Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPECAM]"
	DEFAULT ${OPTION_DEFAULT})

define_channel_server_options(${OPTION_SERVER_DEFAULT})
define_channel_client_options(${OPTION_CLIENT_DEFAULT})

