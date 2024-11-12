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

message(DEPRECATION "TSMF channel is no longer maintained. Use [MS-RDPEVOR] (/video) instead.")

find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(gstreamer gstreamer-1.0)
endif()

if(WITH_GSTREAMER_1_0)
  if(gstreamer_FOUND)
    add_compile_definitions(WITH_GSTREAMER_1_0)
  else()
    message(WARNING "gstreamer not detected, disabling support")
  endif()
endif()

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
    tsmf_types.h
)

set(${MODULE_PREFIX}_LIBS winpr freerdp)
include_directories(..)

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")

if(WITH_VIDEO_FFMPEG)
  add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "ffmpeg" "decoder")
endif()

if(WITH_GSTREAMER_1_0)
  find_package(X11)
  if(X11_Xrandr_FOUND)
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
