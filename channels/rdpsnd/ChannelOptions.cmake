
set(CHANNEL_TYPE "static")
set(CHANNEL_SHORT_NAME "rdpsnd")
set(CHANNEL_LONG_NAME "Audio Output Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPEA]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(WIN32)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
endif()

