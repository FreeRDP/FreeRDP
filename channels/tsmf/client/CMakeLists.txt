# FreeRDP: A Remote Desktop Protocol Implementation
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

define_channel_client("tsmf")

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

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")



target_link_libraries(${MODULE_NAME} freerdp)

if (WITH_DEBUG_SYMBOLS AND MSVC AND NOT STATIC_CHANNELS AND BUILD_SHARED_LIBS)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${FREERDP_ADDIN_PATH} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Client")

if(WITH_FFMPEG)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "ffmpeg" "decoder")
endif()

if(WITH_GSTREAMER_0_10 OR WITH_GSTREAMER_1_0)
	set(XRANDR_FEATURE_TYPE "REQUIRED")
	set(XRANDR_FEATURE_PURPOSE "X11 randr")
	set(XRANDR_FEATURE_DESCRIPTION "X11 randr extension")
	find_feature(XRandR ${XRANDR_FEATURE_TYPE} ${XRANDR_FEATURE_PURPOSE} ${XRANDR_FEATURE_DESCRIPTION})
	if (WITH_XRANDR)
		add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "gstreamer" "decoder")
	else()
		message(WARNING "Disabling tsmf gstreamer because XRandR wasn't found")
	endif()
endif()

if(WITH_OSS)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "oss" "audio")
endif()

if(WITH_ALSA)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "alsa" "audio")
endif()

if(WITH_PULSE)
	add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "pulse" "audio")
endif()
