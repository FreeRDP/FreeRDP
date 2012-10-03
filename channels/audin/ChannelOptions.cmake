
set(CHANNEL_TYPE "dynamic")
set(CHANNEL_SHORT_NAME "audin")
set(CHANNEL_LONG_NAME "Audio Input Redirection Virtual Channel Extension")
set(CHANNEL_SPECIFICATIONS "[MS-RDPEAI]")

string(TOUPPER "WITH_${CHANNEL_SHORT_NAME}" CHANNEL_OPTION)
option(${CHANNEL_OPTION} "Build ${CHANNEL_SHORT_NAME}" ON)


