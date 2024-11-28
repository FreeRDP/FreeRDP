set(OPTION_DEFAULT OFF)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT OFF)

define_channel_options(
  NAME
  "rdpear"
  TYPE
  "dynamic"
  DESCRIPTION
  "Authentication redirection Virtual Channel Extension"
  SPECIFICATIONS
  "[MS-RDPEAR]"
  DEFAULT
  ${OPTION_DEFAULT}
)

define_channel_client_options(${OPTION_CLIENT_DEFAULT})
define_channel_server_options(${OPTION_SERVER_DEFAULT})
