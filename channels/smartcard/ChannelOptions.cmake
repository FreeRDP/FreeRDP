
set(OPTION_DEFAULT OFF)

if(WITH_PCSC)
	set(OPTION_DEFAULT ON)
endif()

define_channel_options(NAME "smartcard" TYPE "device"
	DESCRIPTION "Smart Card Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPESC]"
	DEFAULT ${OPTION_DEFAULT})

