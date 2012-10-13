# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(CHANNEL "audin")
set(MODULE_NAME "${CHANNEL}-server")
set(MODULE_PREFIX "CHANNEL_AUDIN_SERVER")

set(${MODULE_PREFIX}_SRCS
	audin.c)

# audin-server is always built-in

set(${MODULE_PREFIX}_STATIC ON PARENT_SCOPE)
set(${MODULE_PREFIX}_NAME ${MODULE_NAME} PARENT_SCOPE)
set(${MODULE_PREFIX}_CHANNEL ${CHANNEL} PARENT_SCOPE)
set(${MODULE_PREFIX}_ENTRY "VirtualChannelEntry" PARENT_SCOPE)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

if(MONOLITHIC_BUILD)
	set(${MODULE_PREFIX}_LIBS freerdp)
else()
	set(${MODULE_PREFIX}_LIBS freerdp-utils freerdp-channels)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL}/Server")
