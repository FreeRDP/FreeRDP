
set(OPTION_DEFAULT ON)

if(WIN32)
	set(OPTION_DEFAULT ON)
elseif(WITH_CUPS)
	set(OPTION_DEFAULT ON)
else()
	set(OPTION_DEFAULT OFF)
endif()

define_channel_options(NAME "printer" TYPE "device"
	DESCRIPTION "Print Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEPC]"
	DEFAULT ${OPTION_DEFAULT})

