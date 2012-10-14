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

set(MODULE_NAME "rdpsnd")
set(MODULE_PREFIX "CHANNEL_RDPSND_CLIENT")

set(${MODULE_PREFIX}_SRCS
	rdpsnd_main.c
	rdpsnd_main.h)

# rdpsnd is always built-in

set(${MODULE_PREFIX}_STATIC ON PARENT_SCOPE)
set(${MODULE_PREFIX}_NAME ${MODULE_NAME} PARENT_SCOPE)
set(${MODULE_PREFIX}_ENTRY "VirtualChannelEntry" PARENT_SCOPE)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-utils)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${FREERDP_PLUGIN_PATH})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${MODULE_NAME}/Client")

if(WITH_ALSA)
	add_subdirectory(alsa)
endif()

if(WITH_PULSEAUDIO)
	add_subdirectory(pulse)
endif()

if(WITH_MACAUDIO)
	add_subdirectory(MacAudio)
endif()

