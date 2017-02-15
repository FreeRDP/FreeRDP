# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2014 Thincast Technologies Gmbh.
# Copyright 2014 David FORT <contact@hardening-consulting.com>
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

define_channel_server("rdpei")

set(${MODULE_PREFIX}_SRCS
	rdpei_main.c
	rdpei_main.h
	../rdpei_common.c
	../rdpei_common.h
)

add_channel_server_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} FALSE "VirtualChannelEntry")

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})


set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Server")
