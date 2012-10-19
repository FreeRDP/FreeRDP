
set(OPTION_DEFAULT ON)

define_channel_options(NAME "rdpdr" TYPE "static"
	DESCRIPTION "Device Redirection Virtual Channel Extension"
	SPECIFICATIONS "[MS-RDPEFS] [MS-RDPEPC] [MS-RDPESC] [MS-RDPESP]"
	DEFAULT ${OPTION_DEFAULT})

