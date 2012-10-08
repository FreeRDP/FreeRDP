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

set(MODULE_NAME "freerdp-channels-client")
set(MODULE_PREFIX "FREERDP_CHANNELS_CLIENT")

set(${MODULE_PREFIX}_SRCS
	tables.c
	tables.h
	loader.c)

foreach(STATIC_MODULE ${CHANNEL_STATIC_CLIENT_MODULES})

	set(STATIC_MODULE_NAME ${${STATIC_MODULE}_CLIENT_NAME})
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${STATIC_MODULE_NAME})

	if(${${STATIC_MODULE}_CLIENT_ENTRY} STREQUAL "VirtualChannelEntry")
		set(ENTRY_POINT_NAME "${${STATIC_MODULE}_CLIENT_NAME}_${${STATIC_MODULE}_CLIENT_ENTRY}")
		set(ENTRY_POINT_IMPORT "extern int ${ENTRY_POINT_NAME}(PCHANNEL_ENTRY_POINTS pEntryPoints);")
		set(VIRTUAL_CHANNEL_ENTRY_IMPORTS "${VIRTUAL_CHANNEL_ENTRY_IMPORTS}\n${VIRTUAL_CHANNEL_ENTRY_IMPORTS}${ENTRY_POINT_IMPORT}")
		set(VIRTUAL_CHANNEL_ENTRY_TABLE "${VIRTUAL_CHANNEL_ENTRY_TABLE}\n\t{ \"${STATIC_MODULE_NAME}\", ${ENTRY_POINT_NAME} },")
	endif()

	if(${${STATIC_MODULE}_CLIENT_ENTRY} STREQUAL "DeviceServiceEntry")
		set(ENTRY_POINT_NAME "${${STATIC_MODULE}_CLIENT_NAME}_${${STATIC_MODULE}_CLIENT_ENTRY}")
		set(ENTRY_POINT_IMPORT "extern int ${ENTRY_POINT_NAME}(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints);")
		set(DEVICE_SERVICE_ENTRY_IMPORTS "${DEVICE_SERVICE_ENTRY_IMPORTS}\n${DEVICE_SERVICE_ENTRY_IMPORTS}${ENTRY_POINT_IMPORT}")
		set(DEVICE_SERVICE_ENTRY_TABLE "${DEVICE_SERVICE_ENTRY_TABLE}\n\t{ \"${STATIC_MODULE_NAME}\", ${ENTRY_POINT_NAME} },")
	endif()
endforeach()

set(VIRTUAL_CHANNEL_ENTRY_TABLE "${VIRTUAL_CHANNEL_ENTRY_TABLE}\n\t{ \"\", NULL }")
set(DEVICE_SERVICE_ENTRY_TABLE "${DEVICE_SERVICE_ENTRY_TABLE}\n\t{ \"\", NULL }")

configure_file(tables.c.in tables.c)

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/Client")

