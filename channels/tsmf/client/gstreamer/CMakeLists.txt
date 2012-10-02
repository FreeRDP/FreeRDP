# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script for gstreamer plugin
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



set(TSMF_GSTREAMER_SRCS
	tsmf_gstreamer.c
)

include_directories(..)
include_directories(${GSTREAMER_INCLUDE_DIRS})

add_library(tsmf_gstreamer ${TSMF_GSTREAMER_SRCS})
set_target_properties(tsmf_gstreamer PROPERTIES PREFIX "")

if(WITH_MONOLITHIC_BUILD)
	target_link_libraries(tsmf_gstreamer freerdp)
else()
	target_link_libraries(tsmf_gstreamer freerdp-utils)
endif()

target_link_libraries(tsmf_gstreamer ${GSTREAMER_LIBRARIES} gstapp-0.10 gstinterfaces-0.10 Xrandr X11 Xext)

install(TARGETS tsmf_gstreamer DESTINATION ${FREERDP_PLUGIN_PATH})

