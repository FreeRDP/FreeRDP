
set(OPTION_DEFAULT ON)

if(WIN32)
	set(OPTION_DEFAULT OFF)
endif()

define_channel_options(NAME "tsmf" TYPE "dynamic"
	DESCRIPTION "Video Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEV]"
	DEFAULT ${OPTION_DEFAULT})

