# FreeRDP: A Remote Desktop Protocol Client
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

if(WITH_STATIC_PLUGINS)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(FILENAME "ChannelOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
	if(${FILEPATH} MATCHES "^([^/]*)//${FILENAME}")
		string(REGEX REPLACE "^([^/]*)//${FILENAME}" "\\1" DIR ${FILEPATH})
		set(CHANNEL_OPTION)
		include(${FILEPATH})
		if(${CHANNEL_OPTION})
			message(STATUS "Adding ${CHANNEL_TYPE} channel \"${CHANNEL_SHORT_NAME}\": ${CHANNEL_LONG_NAME}")
			add_subdirectory(${DIR})
		endif()
	endif()
endforeach(FILEPATH)

if(WITH_CLIENT_CHANNELS)
	add_subdirectory(client)
endif()

if(WITH_SERVER_CHANNELS)
	add_subdirectory(server)
endif()

