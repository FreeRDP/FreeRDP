# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2023 David Fort <contact@hardening-consulting.com>
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

define_channel_client("rdpear")

if(WIN32)
  # Redirected calls are handled by the native TSSSP LSA package, without krb5.
  set(${MODULE_PREFIX}_SRCS rdpear_sspi.c rdpear_packet.h)
  set(${MODULE_PREFIX}_LIBS winpr freerdp)
elseif(WITH_LSA4POSIX)
  # Redirected calls are serviced by the lsa4posix daemon through the WinPR LSA client.
  set(${MODULE_PREFIX}_SRCS rdpear_sspi.c rdpear_packet.h)
  set(${MODULE_PREFIX}_LIBS winpr freerdp)

  option(WITH_RDPEAR_INPROC_FALLBACK "rdpear: service the calls in process when lsa4posix is not running" ON)
  if(WITH_RDPEAR_INPROC_FALLBACK)
    # A connection that used NLA because the daemon was not running is serviced in process.
    list(APPEND ${MODULE_PREFIX}_SRCS rdpear_inproc.c rdpear_inproc.h)
    list(APPEND ${MODULE_PREFIX}_LIBS rdpear-common)
    add_compile_definitions(WITH_RDPEAR_INPROC_FALLBACK)
  endif()
else()
  set(${MODULE_PREFIX}_SRCS rdpear_main.c rdpear_inproc.c rdpear_inproc.h rdpear_packet.h)
  set(${MODULE_PREFIX}_LIBS winpr freerdp rdpear-common)
endif()

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")
