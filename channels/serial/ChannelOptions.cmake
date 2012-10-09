
set(CHANNEL_TYPE "device")
set(CHANNEL_SHORT_NAME "serial")
set(CHANNEL_LONG_NAME "Serial Port Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPESP]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(WIN32)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
endif()

