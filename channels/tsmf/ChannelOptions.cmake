
set(CHANNEL_TYPE "dynamic")
set(CHANNEL_SHORT_NAME "tsmf")
set(CHANNEL_LONG_NAME "Video Redirection Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPEV]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(WIN32)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
endif()


