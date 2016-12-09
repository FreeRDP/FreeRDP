# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright (c) 2015 Armin Novak <armin.novak@thincast.com>
# Copyright (c) 2015 Thincast Technologies GmbH
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

define_channel_client_subsystem("audin" "mac" "")
FIND_LIBRARY(CORE_AUDIO CoreAudio)
FIND_LIBRARY(AUDIO_TOOL AudioToolbox)
FIND_LIBRARY(APP_SERVICES ApplicationServices)

set(${MODULE_PREFIX}_SRCS
	audin_mac.c)

include_directories(..)
include_directories(${MAC_INCLUDE_DIRS})

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")

set(${MODULE_PREFIX}_LIBS freerdp ${CORE_AUDIO} ${AUDIO_TOOL} ${APP_SERVICES} winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
