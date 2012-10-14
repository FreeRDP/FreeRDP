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

set(CHANNEL_NAME "rdpsnd")
set(MODULE_NAME ${CHANNEL_NAME})
string(TOUPPER "CHANNEL_${CHANNEL_NAME}" MODULE_PREFIX)

if(WITH_CLIENT_CHANNELS)
	add_subdirectory(client)
	if(${MODULE_PREFIX}_CLIENT_STATIC)
		set(CHANNEL_STATIC_CLIENT_MODULES ${CHANNEL_STATIC_CLIENT_MODULES} ${MODULE_PREFIX} PARENT_SCOPE)
		set(${MODULE_PREFIX}_CLIENT_NAME ${CHANNEL_NAME} PARENT_SCOPE)
		set(${MODULE_PREFIX}_CLIENT_CHANNEL ${${MODULE_PREFIX}_CLIENT_CHANNEL} PARENT_SCOPE)
		set(${MODULE_PREFIX}_CLIENT_ENTRY ${${MODULE_PREFIX}_CLIENT_ENTRY} PARENT_SCOPE)
	endif()
endif()

if(WITH_SERVER_CHANNELS)
	add_subdirectory(server)
	if(${MODULE_PREFIX}_SERVER_STATIC)
		set(CHANNEL_STATIC_SERVER_MODULES ${CHANNEL_STATIC_SERVER_MODULES} ${MODULE_PREFIX} PARENT_SCOPE)
		set(${MODULE_PREFIX}_SERVER_NAME ${${MODULE_PREFIX}_SERVER_NAME} PARENT_SCOPE)
		set(${MODULE_PREFIX}_SERVER_CHANNEL ${${MODULE_PREFIX}_SERVER_CHANNEL} PARENT_SCOPE)
		set(${MODULE_PREFIX}_SERVER_ENTRY ${${MODULE_PREFIX}_SERVER_ENTRY} PARENT_SCOPE)
	endif()
endif()
