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

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

macro(add_channel_client _channel_prefix _channel_name)
	add_subdirectory(client)
	if(${_channel_prefix}_CLIENT_STATIC)
		set(CHANNEL_STATIC_CLIENT_MODULES ${CHANNEL_STATIC_CLIENT_MODULES} ${_channel_prefix} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_NAME ${_channel_name} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_CHANNEL ${${_channel_prefix}_CLIENT_CHANNEL} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_ENTRY ${${_channel_prefix}_CLIENT_ENTRY} PARENT_SCOPE)
	endif()
endmacro(add_channel_client)

macro(add_channel_server _channel_prefix _channel_name)
	add_subdirectory(server)
	if(${_channel_prefix}_SERVER_STATIC)
		set(CHANNEL_STATIC_SERVER_MODULES ${CHANNEL_STATIC_SERVER_MODULES} ${_channel_prefix} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_NAME ${_channel_name} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_CHANNEL ${${_channel_prefix}_SERVER_CHANNEL} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_ENTRY ${${_channel_prefix}_SERVER_ENTRY} PARENT_SCOPE)
	endif()
endmacro(add_channel_server)

macro(add_channel_client_library _module_prefix _module_name _plugin _entry)

	if(_plugin AND MSVC AND (NOT STATIC_CHANNELS))
		set(${_module_prefix}_SRCS ${${_module_prefix}_SRCS} module.def)
	endif()
	
	if(_plugin AND (NOT STATIC_CHANNELS))
		add_library(${_module_name} ${${_module_prefix}_SRCS})
	else()
		set(${_module_prefix}_STATIC ON PARENT_SCOPE)
		set(${_module_prefix}_NAME ${_module_name} PARENT_SCOPE)
		set(${_module_prefix}_ENTRY ${_entry} PARENT_SCOPE)
		add_library(${_module_name} STATIC ${${_module_prefix}_SRCS})
	endif()
	
endmacro(add_channel_client_library)

macro(add_channel_server_library _module_prefix _module_name _plugin _entry)

	if(_plugin AND MSVC AND (NOT STATIC_CHANNELS))
		set(${_module_prefix}_SRCS ${${_module_prefix}_SRCS} module.def)
	endif()
	
	if(_plugin AND (NOT STATIC_CHANNELS))
		add_library(${_module_name} ${${_module_prefix}_SRCS})
	else()
		set(${_module_prefix}_STATIC ON PARENT_SCOPE)
		set(${_module_prefix}_NAME ${_module_name} PARENT_SCOPE)
		set(${_module_prefix}_ENTRY ${_entry} PARENT_SCOPE)
		add_library(${_module_name} STATIC ${${_module_prefix}_SRCS})
	endif()
	
endmacro(add_channel_server_library)

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

