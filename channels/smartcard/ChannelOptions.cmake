
set(CHANNEL_TYPE "device")
set(CHANNEL_SHORT_NAME "smartcard")
set(CHANNEL_LONG_NAME "Smart Card Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPESC]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)

if(WIN32)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
elseif(WITH_PCSC)
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)
else()
	option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" OFF)
endif()

