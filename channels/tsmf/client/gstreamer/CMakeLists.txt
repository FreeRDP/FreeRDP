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

if(NOT GSTREAMER_0_10_FOUND AND NOT GSTREAMER_1_0_FOUND)
	message(FATAL_ERROR "GStreamer library not found, but required for TSMF module.")
elseif (GSTREAMER_0_10_FOUND AND GSTREAMER_1_0_FOUND)
	message(FATAL_ERROR "GStreamer 0.10 and GStreamer 1.0 support are mutually exclusive!")
endif()

set(SRC "tsmf_gstreamer.c")

if (GSTREAMER_1_0_FOUND)
	set(LIBS ${GSTREAMER_1_0_LIBRARIES})
	include_directories(${GSTREAMER_1_0_INCLUDE_DIRS})
elseif (GSTREAMER_0_10_FOUND)
	set(LIBS ${GSTREAMER_0_10_LIBRARIES})
	include_directories(${GSTREAMER_0_10_INCLUDE_DIRS})
endif()

if(ANDROID)
	set(SRC ${SRC}
		tsmf_android.c)
	set(LIBS ${LIBS})
else()
	set(XEXT_FEATURE_TYPE "RECOMMENDED")
	set(XEXT_FEATURE_PURPOSE "X11 extension")
	set(XEXT_FEATURE_DESCRIPTION "X11 core extensions")

	find_feature(Xext ${XEXT_FEATURE_TYPE} ${XEXT_FEATURE_PURPOSE} ${XEXT_FEATURE_DESCRIPTION})

	set(SRC ${SRC}
		tsmf_X11.c)
	set(LIBS ${LIBS} ${X11_LIBRARIES} ${XEXT_LIBRARIES})

	if(XEXT_FOUND)
		add_definitions(-DWITH_XEXT=1)
	endif()

endif()

set(${MODULE_PREFIX}_SRCS "${SRC}")

include_directories(..)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")



target_link_libraries(${MODULE_NAME} ${LIBS} freerdp)
