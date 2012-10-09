# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
#
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

set(MODULE_NAME "rail")
set(MODULE_PREFIX "CHANNEL_RAIL_CLIENT")

set(${MODULE_PREFIX}_SRCS
	rail_main.c
	rail_main.h
	rail_orders.c
	rail_orders.h)

# rail is always built-in

set(${MODULE_PREFIX}_STATIC ON PARENT_SCOPE)
set(${MODULE_PREFIX}_NAME ${MODULE_NAME} PARENT_SCOPE)
set(${MODULE_PREFIX}_ENTRY "VirtualChannelEntry" PARENT_SCOPE)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

if(WITH_MONOLITHIC_BUILD)
	target_link_libraries(${MODULE_NAME} freerdp)
else()
	target_link_libraries(${MODULE_NAME} freerdp-utils)
endif()

install(TARGETS ${MODULE_NAME} DESTINATION ${FREERDP_PLUGIN_PATH})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${MODULE_NAME}/Client")

