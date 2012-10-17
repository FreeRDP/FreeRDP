
set(CHANNEL_TYPE "device")
set(CHANNEL_SHORT_NAME "disk")
set(CHANNEL_LONG_NAME "Disk Redirection Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPEFS]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(ANDROID)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
endif()


