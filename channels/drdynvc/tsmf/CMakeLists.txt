# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(TSMF_SRCS
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
	tsmf_types.h
)

include_directories(..)

add_library(tsmf ${TSMF_SRCS})
set_target_properties(tsmf PROPERTIES PREFIX "")

target_link_libraries(tsmf freerdp-utils)

install(TARGETS tsmf DESTINATION ${FREERDP_PLUGIN_PATH})

if(WITH_FFMPEG)
	add_subdirectory(ffmpeg)
endif()

if(WITH_ALSA)
	add_subdirectory(alsa)
endif()

if(WITH_PULSEAUDIO)
	add_subdirectory(pulse)
endif()

