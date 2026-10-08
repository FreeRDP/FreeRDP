define_channel_client("rdpexps")

set(${MODULE_PREFIX}_SRCS rdpexps_main.c rdpexps_main.h rdpexps_common.c rdpexps_common.h)
set(${MODULE_PREFIX}_LIBS winpr)
include_directories(..)
add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
