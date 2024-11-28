set(OPTION_DEFAULT ON)
set(OPTION_CLIENT_DEFAULT OFF)
set(OPTION_SERVER_DEFAULT ON)

define_channel_options(
  NAME
  "rdpemsc"
  TYPE
  "dynamic"
  DESCRIPTION
  "Mouse Cursor Virtual Channel Extension"
  SPECIFICATIONS
  "[MS-RDPEMSC]"
  DEFAULT
  ${OPTION_DEFAULT}
)

define_channel_server_options(${OPTION_SERVER_DEFAULT})
