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

include(CMakeParseArguments)
include(CMakeDependentOption)

macro(define_channel_options)
	set(PREFIX "CHANNEL")

	cmake_parse_arguments(${PREFIX}
		""
		"NAME;TYPE;DESCRIPTION;SPECIFICATIONS;DEFAULT"
		""
		${ARGN})

	string(TOUPPER "CHANNEL_${CHANNEL_NAME}" CHANNEL_OPTION)
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT" CHANNEL_CLIENT_OPTION)
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_SERVER" CHANNEL_SERVER_OPTION)
	
	if(${${CHANNEL_CLIENT_OPTION}})
		set(OPTION_CLIENT_DEFAULT ${${CHANNEL_CLIENT_OPTION}})
	endif()
	
	if(${${CHANNEL_SERVER_OPTION}})
		set(OPTION_SERVER_DEFAULT ${${CHANNEL_SERVER_OPTION}})
	endif()
	
	if(${${CHANNEL_OPTION}})
		set(OPTION_DEFAULT ${${CHANNEL_OPTION}})
	endif()
	
	if(${OPTION_CLIENT_DEFAULT} OR ${OPTION_SERVER_DEFAULT})
		set(OPTION_DEFAULT "ON")
	endif()
	
	set(CHANNEL_DEFAULT ${OPTION_DEFAULT})
		
	set(CHANNEL_OPTION_DOC "Build ${CHANNEL_NAME} ${CHANNEL_TYPE} channel")
	option(${CHANNEL_OPTION} "${CHANNEL_OPTION_DOC}" ${CHANNEL_DEFAULT})

endmacro(define_channel_options)

macro(define_channel_client_options _channel_client_default)
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT" CHANNEL_CLIENT_OPTION)
	set(CHANNEL_CLIENT_OPTION_DOC "Build ${CHANNEL_NAME} ${CHANNEL_TYPE} channel client")
	option(${CHANNEL_CLIENT_OPTION} "${CHANNEL_CLIENT_OPTION_DOC}" ${_channel_client_default})
	cmake_dependent_option(${CHANNEL_CLIENT_OPTION} "${CHANNEL_CLIENT_OPTION_DOC}"
		${_channel_client_default} "${CHANNEL_OPTION}" OFF)
endmacro(define_channel_client_options)

macro(define_channel_server_options _channel_server_default)
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_SERVER" CHANNEL_SERVER_OPTION)
	set(CHANNEL_SERVER_OPTION_DOC "Build ${CHANNEL_NAME} ${CHANNEL_TYPE} channel server")
	option(${CHANNEL_SERVER_OPTION} "${CHANNEL_SERVER_OPTION_DOC}" ${_channel_server_default})
	cmake_dependent_option(${CHANNEL_SERVER_OPTION} "${CHANNEL_SERVER_OPTION_DOC}"
		${_channel_server_default} "${CHANNEL_OPTION}" OFF)
endmacro(define_channel_server_options)

macro(define_channel _channel_name)
	set(CHANNEL_NAME ${_channel_name})
	set(MODULE_NAME ${CHANNEL_NAME})
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}" MODULE_PREFIX)
endmacro(define_channel)

macro(define_channel_client _channel_name)
	set(CHANNEL_NAME ${_channel_name})
	set(MODULE_NAME "${CHANNEL_NAME}-client")
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT" MODULE_PREFIX)
endmacro(define_channel_client)

macro(define_channel_server _channel_name)
	set(CHANNEL_NAME ${_channel_name})
	set(MODULE_NAME "${CHANNEL_NAME}-server")
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_SERVER" MODULE_PREFIX)
endmacro(define_channel_server)

macro(define_channel_client_subsystem _channel_name _subsystem _type)
	set(CHANNEL_NAME ${_channel_name})
	set(CHANNEL_SUBSYSTEM ${_subsystem})
	string(LENGTH "${_type}" _type_length)
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT" CHANNEL_PREFIX)
	if(_type_length GREATER 0)
		set(SUBSYSTEM_TYPE ${_type})
		set(MODULE_NAME "${CHANNEL_NAME}-client-${CHANNEL_SUBSYSTEM}-${SUBSYSTEM_TYPE}")
		string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT_${CHANNEL_SUBSYSTEM}_${SUBSYSTEM_TYPE}" MODULE_PREFIX)
	else()
		set(MODULE_NAME "${CHANNEL_NAME}-client-${CHANNEL_SUBSYSTEM}")
		string(TOUPPER "CHANNEL_${CHANNEL_NAME}_CLIENT_${CHANNEL_SUBSYSTEM}" MODULE_PREFIX)
	endif()
endmacro(define_channel_client_subsystem)

macro(define_channel_server_subsystem _channel_name _subsystem _type)
	set(CHANNEL_NAME ${_channel_name})
	set(CHANNEL_SUBSYSTEM ${_subsystem})
	set(MODULE_NAME "${CHANNEL_NAME}-server-${CHANNEL_SUBSYSTEM}")
	string(TOUPPER "CHANNEL_${CHANNEL_NAME}_server_${CHANNEL_SUBSYSTEM}" MODULE_PREFIX)
endmacro(define_channel_server_subsystem)

macro(add_channel_client _channel_prefix _channel_name)
	add_subdirectory(client)
	if(${${_channel_prefix}_CLIENT_STATIC})
		set(CHANNEL_STATIC_CLIENT_MODULES ${CHANNEL_STATIC_CLIENT_MODULES} ${_channel_prefix} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_NAME ${${_channel_prefix}_CLIENT_NAME} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_CHANNEL ${${_channel_prefix}_CLIENT_CHANNEL} PARENT_SCOPE)
		set(${_channel_prefix}_CLIENT_ENTRY ${${_channel_prefix}_CLIENT_ENTRY} PARENT_SCOPE)
		set(CHANNEL_STATIC_CLIENT_ENTRIES ${CHANNEL_STATIC_CLIENT_ENTRIES} ${${_channel_prefix}_CLIENT_ENTRY} PARENT_SCOPE)
	endif()
endmacro(add_channel_client)

macro(add_channel_server _channel_prefix _channel_name)
	add_subdirectory(server)
	if(${${_channel_prefix}_SERVER_STATIC})
		set(CHANNEL_STATIC_SERVER_MODULES ${CHANNEL_STATIC_SERVER_MODULES} ${_channel_prefix} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_NAME ${${_channel_prefix}_SERVER_NAME} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_CHANNEL ${${_channel_prefix}_SERVER_CHANNEL} PARENT_SCOPE)
		set(${_channel_prefix}_SERVER_ENTRY ${${_channel_prefix}_SERVER_ENTRY} PARENT_SCOPE)
		set(CHANNEL_STATIC_SERVER_ENTRIES ${CHANNEL_STATIC_SERVER_ENTRIES} ${${_channel_prefix}_SERVER_ENTRY} PARENT_SCOPE)
	endif()
endmacro(add_channel_server)

macro(add_channel_client_subsystem _channel_prefix _channel_name _subsystem _type)
	add_subdirectory(${_subsystem})
	set(_channel_module_name "${_channel_name}-client")
	string(LENGTH "${_type}" _type_length)
	if(_type_length GREATER 0)
		string(TOUPPER "CHANNEL_${_channel_name}_CLIENT_${_subsystem}_${_type}" _subsystem_prefix)
	else()
		string(TOUPPER "CHANNEL_${_channel_name}_CLIENT_${_subsystem}" _subsystem_prefix)
	endif()
	if(${${_subsystem_prefix}_STATIC})
		get_target_property(CHANNEL_SUBSYSTEMS ${_channel_module_name} SUBSYSTEMS)
		if(_type_length GREATER 0)
			set(SUBSYSTEMS ${SUBSYSTEMS} "${_subsystem}-${_type}")
		else()
			set(SUBSYSTEMS ${SUBSYSTEMS} ${_subsystem})
		endif()
		set_target_properties(${_channel_module_name} PROPERTIES SUBSYSTEMS "${SUBSYSTEMS}")
	endif()
endmacro(add_channel_client_subsystem)

macro(channel_install _targets _destination _export_target)
		install(TARGETS ${_targets} DESTINATION ${_destination} EXPORT ${_export_target})
endmacro(channel_install)

macro(server_channel_install _targets _destination)
	channel_install(${_targets} ${_destination} "FreeRDP-ServerTargets")
endmacro(server_channel_install)

macro(client_channel_install _targets _destination)
	channel_install(${_targets} ${_destination} "FreeRDP-ClientTargets")
endmacro(client_channel_install)

macro(add_channel_client_library _module_prefix _module_name _channel_name _dynamic _entry)
	if(${_dynamic} AND (NOT BUILTIN_CHANNELS))
# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
    if (WIN32)
      set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
      set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
      set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
      set (RC_VERSION_PATCH 0)
      set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${_module_name}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

      configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

      set ( ${_module_prefix}_SRCS ${${_module_prefix}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
    endif()

		add_library(${_module_name} ${${_module_prefix}_SRCS})
		client_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
	else()
		set(${_module_prefix}_STATIC ON PARENT_SCOPE)
		set(${_module_prefix}_NAME ${_module_name} PARENT_SCOPE)
		set(${_module_prefix}_CHANNEL ${_channel_name} PARENT_SCOPE)
		set(${_module_prefix}_ENTRY ${_entry} PARENT_SCOPE)
		add_library(${_module_name} STATIC ${${_module_prefix}_SRCS})
		if (${CMAKE_VERSION} VERSION_LESS 2.8.12 OR NOT BUILD_SHARED_LIBS)
			client_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
		endif()
	endif()
endmacro(add_channel_client_library)

macro(add_channel_client_subsystem_library _module_prefix _module_name _channel_name _type _dynamic _entry)
	if(${_dynamic} AND (NOT BUILTIN_CHANNELS))
# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
    if (WIN32)
      set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
      set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
      set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
      set (RC_VERSION_PATCH 0)
      set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${_module_name}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

      configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

      set ( ${_module_prefix}_SRCS ${${_module_prefix}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
    endif()

		add_library(${_module_name} ${${_module_prefix}_SRCS})
		client_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
	else()
		set(${_module_prefix}_STATIC ON PARENT_SCOPE)
		set(${_module_prefix}_NAME ${_module_name} PARENT_SCOPE)
		set(${_module_prefix}_TYPE ${_type} PARENT_SCOPE)
		add_library(${_module_name} STATIC ${${_module_prefix}_SRCS})
		if (${CMAKE_VERSION} VERSION_LESS 2.8.12 OR NOT BUILD_SHARED_LIBS)
			client_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
		endif()
	endif()
endmacro(add_channel_client_subsystem_library)

macro(add_channel_server_library _module_prefix _module_name _channel_name _dynamic _entry)
	if(${_dynamic} AND (NOT BUILTIN_CHANNELS))
# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
    if (WIN32)
      set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
      set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
      set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
      set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${_module_name}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

      configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

      set ( ${_module_prefix}_SRCS ${${_module_prefix}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
    endif()

		add_library(${_module_name} ${${_module_prefix}_SRCS})
		server_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
	else()
		set(${_module_prefix}_STATIC ON PARENT_SCOPE)
		set(${_module_prefix}_NAME ${_module_name} PARENT_SCOPE)
		set(${_module_prefix}_CHANNEL ${_channel_name} PARENT_SCOPE)
		set(${_module_prefix}_ENTRY ${_entry} PARENT_SCOPE)
		add_library(${_module_name} STATIC ${${_module_prefix}_SRCS})
		if (${CMAKE_VERSION} VERSION_LESS 2.8.12 OR NOT BUILD_SHARED_LIBS)
			server_channel_install(${_module_name}  ${FREERDP_ADDIN_PATH})
		endif()
	endif()
endmacro(add_channel_server_library)

set(FILENAME "ChannelOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
	if(${FILEPATH} MATCHES "^([^/]*)/+${FILENAME}")
		string(REGEX REPLACE "^([^/]*)/+${FILENAME}" "\\1" DIR ${FILEPATH})
		set(CHANNEL_OPTION)
		include(${FILEPATH})
		if(${CHANNEL_OPTION})
			set(CHANNEL_MESSAGE "Adding ${CHANNEL_TYPE} channel")
			if(${CHANNEL_CLIENT_OPTION})
				set(CHANNEL_MESSAGE "${CHANNEL_MESSAGE} client")
			endif()
			if(${CHANNEL_SERVER_OPTION})
				set(CHANNEL_MESSAGE "${CHANNEL_MESSAGE} server")
			endif()
			set(CHANNEL_MESSAGE "${CHANNEL_MESSAGE} \"${CHANNEL_NAME}\"")
			set(CHANNEL_MESSAGE "${CHANNEL_MESSAGE}: ${CHANNEL_DESCRIPTION}")
			message(STATUS "${CHANNEL_MESSAGE}")
			add_subdirectory(${DIR})
		endif()
	endif()
endforeach(FILEPATH)

if(WITH_CLIENT_CHANNELS)
	add_subdirectory(client)
	set(FREERDP_CHANNELS_CLIENT_SRCS ${FREERDP_CHANNELS_CLIENT_SRCS} PARENT_SCOPE)
	set(FREERDP_CHANNELS_CLIENT_LIBS ${FREERDP_CHANNELS_CLIENT_LIBS} PARENT_SCOPE)
endif()

if(WITH_SERVER_CHANNELS)
	add_subdirectory(server)
	set(FREERDP_CHANNELS_SERVER_SRCS ${FREERDP_CHANNELS_SERVER_SRCS} PARENT_SCOPE)
	set(FREERDP_CHANNELS_SERVER_LIBS ${FREERDP_CHANNELS_SERVER_LIBS} PARENT_SCOPE)
endif()
