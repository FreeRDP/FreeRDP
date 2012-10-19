
set(OPTION_DEFAULT ON)

if(ANDROID)
	set(OPTION_DEFAULT OFF)
endif()

define_channel_options(NAME "disk" TYPE "device"
	DESCRIPTION "Disk Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEFS]"
	DEFAULT ${OPTION_DEFAULT})

