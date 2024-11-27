set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT ON)
set(OPTION_SERVER_DEFAULT OFF)

if(WIN32)
  set(OPTION_CLIENT_DEFAULT ON)
  set(OPTION_SERVER_DEFAULT OFF)
else()
  # cups is available on mac os and linux by default, on android it is optional.
  if(NOT IOS AND NOT ANDROID)
    set(CUPS_DEFAULT ON)
  else()
    set(CUPS_DEFAULT OFF)
  endif()
  option(WITH_CUPS "CUPS printer support" ${CUPS_DEFAULT})
  if(WITH_CUPS)
    set(OPTION_CLIENT_DEFAULT ON)
    set(OPTION_SERVER_DEFAULT OFF)
  else()
    set(OPTION_CLIENT_DEFAULT OFF)
    set(OPTION_SERVER_DEFAULT OFF)
  endif()
endif()

define_channel_options(
  NAME
  "printer"
  TYPE
  "device"
  DESCRIPTION
  "Print Virtual Channel Extension"
  SPECIFICATIONS
  "[MS-RDPEPC]"
  DEFAULT
  ${OPTION_DEFAULT}
)

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})
