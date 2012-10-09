# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2012 Hewlett-Packard Development Company, L.P.
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

set(MODULE_NAME "tsmf")
set(MODULE_PREFIX "CHANNEL_TSMF_CLIENT")

set(${MODULE_PREFIX}_SRCS
	tsmf_audio.c
	tsmf_audio.h
	tsmf_codec.c
	tsmf_codec.h
	tsmf_constants.h
	tsmf_decoder.c
	tsmf_decoder.h
	tsmf_ifman.c
	tsmf_ifman.h
	tsmf_main.c
	tsmf_main.h
	tsmf_media.c
	tsmf_media.h
	tsmf_types.h)

include_directories(..)

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

if(MONOLITHIC_BUILD)
	target_link_libraries(${MODULE_NAME} freerdp)
else()
	target_link_libraries(${MODULE_NAME} freerdp-utils)
endif()

install(TARGETS ${MODULE_NAME} DESTINATION ${FREERDP_PLUGIN_PATH})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${MODULE_NAME}/Client")

if(WITH_FFMPEG)
	add_subdirectory(ffmpeg)
endif()

if(WITH_XRANDR)
	if(GSTREAMER_FOUND)
		add_subdirectory(gstreamer)
	endif()
endif()

if(WITH_ALSA)
	add_subdirectory(alsa)
endif()

if(WITH_PULSEAUDIO)
	add_subdirectory(pulse)
endif()
