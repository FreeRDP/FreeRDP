
set(OPTION_DEFAULT ON)

if(WIN32)
	set(OPTION_DEFAULT OFF)
endif()

define_channel_options(NAME "serial" TYPE "device"
	DESCRIPTION "Serial Port Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPESP]"
	DEFAULT ${OPTION_DEFAULT})

