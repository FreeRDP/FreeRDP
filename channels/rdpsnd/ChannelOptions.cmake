
set(OPTION_DEFAULT ON)

define_channel_options(NAME "rdpsnd" TYPE "static"
	DESCRIPTION "Audio Output Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEA]"
	DEFAULT ${OPTION_DEFAULT})

