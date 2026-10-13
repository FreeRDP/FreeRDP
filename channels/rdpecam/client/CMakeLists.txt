# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2024 Oleg Turovski <oleg2104@hotmail.com>
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

define_channel_client("rdpecam")

# the movie backend plays a video file as a camera (/dvc:rdpecam,sys:movie,file:<path>)
option(WITH_RDPECAM_MOVIE "Build the rdpecam movie backend (requires FFmpeg)" ON)
if(WITH_RDPECAM_MOVIE)
  find_package(FFmpeg COMPONENTS AVFORMAT AVCODEC AVUTIL SWSCALE)
  if(NOT FFmpeg_FOUND)
    message(STATUS "FFmpeg (avformat, avcodec, avutil, swscale) not found, rdpecam movie backend disabled")
    set(WITH_RDPECAM_MOVIE OFF)
  endif()
endif()

# the gst backend plays a GStreamer pipeline as a camera (/dvc:rdpecam,sys:gst,pipeline:<pipeline>)
# not on Android: there pkg-config would happily return the host GStreamer
cmake_dependent_option(WITH_RDPECAM_GSTREAMER "Build the rdpecam GStreamer backend" ON "NOT ANDROID" OFF)
if(WITH_RDPECAM_GSTREAMER)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(RDPECAM_GST gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
  endif()
  if(NOT RDPECAM_GST_FOUND)
    message(STATUS "GStreamer (gstreamer, app, video) not found, rdpecam gst backend disabled")
    set(WITH_RDPECAM_GSTREAMER OFF)
  endif()
endif()

# currently camera redirect client supported for platforms with Video4Linux or Android
if(NOT ANDROID)
  find_package(V4L)
  if(V4L_FOUND)
    set(WITH_V4L ON)
    add_compile_definitions("WITH_V4L")
  elseif(NOT WITH_RDPECAM_MOVIE AND NOT WITH_RDPECAM_GSTREAMER)
    message(FATAL_ERROR "libv4l-dev required for CHANNEL_RDPECAM_CLIENT")
  endif()
endif()

set(${MODULE_PREFIX}_LIBS freerdp winpr)

set(${MODULE_PREFIX}_SRCS camera_device_enum_main.c camera_device_main.c encoding.c)

# TRUE = build as static addin (linked into libfreerdp-client), not dynamic plugin (.so)
add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")

if(WITH_V4L)
  add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "v4l" "")
endif()

if(ANDROID)
  add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "android" "")
endif()

if(WITH_RDPECAM_MOVIE)
  add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "movie" "")
endif()

if(WITH_RDPECAM_GSTREAMER)
  add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "gst" "")
endif()
