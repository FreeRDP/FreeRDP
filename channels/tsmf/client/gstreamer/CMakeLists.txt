# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script for gstreamer subsystem
#
# (C) Copyright 2012 Hewlett-Packard Development Company, L.P.
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

define_channel_client_subsystem("tsmf" "gstreamer" "decoder")

set(${MODULE_PREFIX}_SRCS
	tsmf_gstreamer.c)

include_directories(..)
include_directories(${GSTREAMER_INCLUDE_DIRS})

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-utils)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS}
	${GSTREAMER_LIBRARIES}
	gstapp-0.10
	gstinterfaces-0.10
	Xrandr X11 Xext)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if(NOT STATIC_CHANNELS)
	install(TARGETS ${MODULE_NAME} DESTINATION ${FREERDP_ADDIN_PATH})
endif()
