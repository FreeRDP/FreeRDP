# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

define_channel_client("rdpsnd")

set(${MODULE_PREFIX}_SRCS
	rdpsnd_main.c
	rdpsnd_main.h)

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} FALSE "VirtualChannelEntry")



target_link_libraries(${MODULE_NAME} winpr freerdp ${CMAKE_THREAD_LIBS_INIT})


set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Client")

if(WITH_OSS)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "oss" "")
endif()

if(WITH_ALSA)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "alsa" "")
endif()

if(WITH_IOSAUDIO)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "ios" "")
endif()

if(WITH_PULSE)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "pulse" "")
endif()

if(WITH_MACAUDIO)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "mac" "")
endif()

if(WITH_WINMM)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "winmm" "")
endif()

if(WITH_OPENSLES)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "opensles" "")
endif()
