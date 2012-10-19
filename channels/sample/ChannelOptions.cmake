
set(OPTION_DEFAULT OFF)

if(WITH_SAMPLE)
	set(OPTION_DEFAULT ON)
endif()

define_channel_options(NAME "sample" TYPE "static"
	DESCRIPTION "Sample Virtual Channel Extension"
	SPECIFICATIONS ""
	DEFAULT ${OPTION_DEFAULT})

