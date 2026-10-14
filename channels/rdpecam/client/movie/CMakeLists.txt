# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2026 David Fort <contact@hardening-consulting.com>
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

define_channel_client_subsystem("rdpecam" "movie" "")

freerdp_client_pc_add_requires_private("libavformat" "libavcodec" "libavutil" "libswscale")
include_directories(
  SYSTEM ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS}
)

set(${MODULE_PREFIX}_SRCS camera_movie.c)

set(${MODULE_PREFIX}_LIBS winpr freerdp ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${SWSCALE_LIBRARIES}
                          ${AVUTIL_LIBRARIES}
)

include_directories(..)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")
