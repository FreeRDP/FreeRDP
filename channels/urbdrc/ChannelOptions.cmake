
set(CHANNEL_TYPE "dynamic")
set(CHANNEL_SHORT_NAME "urbdrc")
set(CHANNEL_LONG_NAME "USB Devices Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPEUSB]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(WIN32)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
endif()


