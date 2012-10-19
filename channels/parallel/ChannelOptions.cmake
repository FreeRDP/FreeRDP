
set(OPTION_DEFAULT ON)

if(WIN32)
	set(OPTION_DEFAULT OFF)
endif()

define_channel_options(NAME "parallel" TYPE "device"
	DESCRIPTION "Parallel Port Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPESP]"
	DEFAULT ${OPTION_DEFAULT})

