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

set(MODULE_NAME "freerdp-channels-client")
set(MODULE_PREFIX "FREERDP_CHANNELS_CLIENT")

set(${MODULE_PREFIX}_SRCS
	tables.c
	tables.h
	channels.c)

list(REMOVE_DUPLICATES CHANNEL_STATIC_CLIENT_ENTRIES)

foreach(STATIC_ENTRY ${CHANNEL_STATIC_CLIENT_ENTRIES})
	
	foreach(STATIC_MODULE ${CHANNEL_STATIC_CLIENT_MODULES})
	
		if(${${STATIC_MODULE}_CLIENT_ENTRY} STREQUAL ${STATIC_ENTRY})
			set(STATIC_MODULE_NAME ${${STATIC_MODULE}_CLIENT_NAME})
			set(STATIC_MODULE_CHANNEL ${${STATIC_MODULE}_CLIENT_CHANNEL})
			set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${STATIC_MODULE_NAME})
			
			set(ENTRY_POINT_NAME "${STATIC_MODULE_CHANNEL}_${${STATIC_MODULE}_CLIENT_ENTRY}")
			set(ENTRY_POINT_IMPORT "extern void ${ENTRY_POINT_NAME}();")
			set(${STATIC_ENTRY}_IMPORTS "${${STATIC_ENTRY}_IMPORTS}\n${ENTRY_POINT_IMPORT}")
			set(${STATIC_ENTRY}_TABLE "${${STATIC_ENTRY}_TABLE}\n\t{ \"${STATIC_MODULE_NAME}\", ${ENTRY_POINT_NAME} },")
		endif()
		
	endforeach()
	
endforeach()

set(CLIENT_STATIC_ENTRY_TABLES_LIST "${CLIENT_STATIC_ENTRY_TABLES_LIST}\nconst STATIC_ENTRY_TABLE CLIENT_STATIC_ENTRY_TABLES[] =\n{")

foreach(STATIC_ENTRY ${CHANNEL_STATIC_CLIENT_ENTRIES})

	set(CLIENT_STATIC_ENTRY_IMPORTS "${CLIENT_STATIC_ENTRY_IMPORTS}\n${${STATIC_ENTRY}_IMPORTS}")

	set(CLIENT_STATIC_ENTRY_TABLES "${CLIENT_STATIC_ENTRY_TABLES}\nconst STATIC_ENTRY CLIENT_${STATIC_ENTRY}_TABLE[] =\n{")
	set(CLIENT_STATIC_ENTRY_TABLES "${CLIENT_STATIC_ENTRY_TABLES}\n${${STATIC_ENTRY}_TABLE}")
	set(CLIENT_STATIC_ENTRY_TABLES "${CLIENT_STATIC_ENTRY_TABLES}\n\t{ \"\", NULL }\n};")
	
	set(CLIENT_STATIC_ENTRY_TABLES_LIST "${CLIENT_STATIC_ENTRY_TABLES_LIST}\n\t{ \"${STATIC_ENTRY}\", CLIENT_${STATIC_ENTRY}_TABLE },")
endforeach()

set(CLIENT_STATIC_ENTRY_TABLES_LIST "${CLIENT_STATIC_ENTRY_TABLES_LIST}\n\t{ \"\", NULL }\n};")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tables.c.in ${CMAKE_CURRENT_SOURCE_DIR}/tables.c)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD} INTERNAL
	MODULE freerdp
	MODULES freerdp-utils)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-crt winpr-synch winpr-interlocked)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/Client")

