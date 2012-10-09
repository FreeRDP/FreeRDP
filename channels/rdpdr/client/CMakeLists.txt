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

set(MODULE_NAME "rdpdr")
set(MODULE_PREFIX "CHANNEL_RDPDR_CLIENT")

set(${MODULE_PREFIX}_SRCS
	irp.c
	irp.h
	devman.c
	devman.h
	rdpdr_main.c
	rdpdr_main.h
	rdpdr_capabilities.c
	rdpdr_capabilities.h)

# rdpdr is always built-in

set(${MODULE_PREFIX}_STATIC ON PARENT_SCOPE)
set(${MODULE_PREFIX}_NAME ${MODULE_NAME} PARENT_SCOPE)
set(${MODULE_PREFIX}_ENTRY "VirtualChannelEntry" PARENT_SCOPE)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

if(MONOLITHIC_BUILD)
	target_link_libraries(${MODULE_NAME} freerdp winpr)
else()
	target_link_libraries(${MODULE_NAME} freerdp-utils winpr-crt winpr-synch winpr-thread winpr-interlocked)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${MODULE_NAME}/Client")

