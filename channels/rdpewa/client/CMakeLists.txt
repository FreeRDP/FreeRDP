# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

define_channel_client("rdpewa")

set(${MODULE_PREFIX}_SRCS
  rdpewa_main.c
  rdpewa_main.h
  rdpewa_cbor.c
  rdpewa_cbor.h
  rdpewa_fido.c
  rdpewa_fido.h
)

set(${MODULE_PREFIX}_LIBS
  winpr
  freerdp
  ${LIBCBOR_LIBRARIES}
  ${LIBFIDO2_LIBRARIES}
)

include_directories(..)
include_directories(${LIBCBOR_INCLUDE_DIRS})
include_directories(${LIBFIDO2_INCLUDE_DIRS})

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")
