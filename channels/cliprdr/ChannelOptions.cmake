
set(OPTION_DEFAULT ON)

define_channel_options(NAME "cliprdr" TYPE "static"
	DESCRIPTION "Clipboard Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPECLIP]"
	DEFAULT ${OPTION_DEFAULT})

