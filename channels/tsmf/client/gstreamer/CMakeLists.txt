# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script for gstreamer subsystem
#
# (C) Copyright 2012 Hewlett-Packard Development Company, L.P.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#		 http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

define_channel_client_subsystem("tsmf" "gstreamer" "decoder")

if(NOT gstreamer_FOUND)
  message(FATAL_ERROR "GStreamer library not found, but required for TSMF module.")
endif()

set(SRC "tsmf_gstreamer.c")

pkg_check_modules(gstreamerbase gstreamer-base-1.0 REQUIRED)
pkg_check_modules(gstreamervideo gstreamer-video-1.0 REQUIRED)
pkg_check_modules(gstreamerapp gstreamer-app-1.0 REQUIRED)

set(LIBS ${gstreamer_LIBRARIES} ${gstreamerbase_LIBRARIES} ${gstreamervideo_LIBRARIES} ${gstreamerapp_LIBRARIES})
include_directories(
  SYSTEM ${gstreamer_INCLUDE_DIRS} ${gstreamerbase_INCLUDE_DIRS} ${gstreamervideo_INCLUDE_DIRS}
  ${gstreamerapp_INCLUDE_DIRS}
)

if(ANDROID)
  set(SRC ${SRC} tsmf_android.c)
else()
  find_package(X11 REQUIRED)

  list(APPEND SRC tsmf_X11.c)
  list(APPEND LIBS ${X11_LIBRARIES} ${X11_Xext_LIB})
  if(NOT APPLE)
    list(APPEND LIBS rt)
  endif()

  if(X11_Xext_FOUND)
    add_compile_definitions(WITH_XEXT=1)
  endif()

endif()

set(${MODULE_PREFIX}_SRCS "${SRC}")

set(${MODULE_PREFIX}_LIBS ${LIBS} winpr)

include_directories(..)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")
