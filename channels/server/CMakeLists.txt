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

set(MODULE_NAME "freerdp-channels-server")
set(MODULE_PREFIX "FREERDP_CHANNELS_SERVER")

foreach(_MODULE_NAME ${CHANNEL_BUILTIN_SERVER_MODULES})
	string(TOUPPER "CHANNEL_${_MODULE_NAME}" _MODULE_PREFIX)
	message(STATUS "Adding built-in channel server module: ${_MODULE_NAME}")

	foreach(SRC ${${_MODULE_PREFIX}_SERVER_SRCS})
		set(CHANNEL_SERVER_SRCS ${CHANNEL_SERVER_SRCS} "../${_MODULE_NAME}/server/${SRC}")
	endforeach()

	set(CHANNEL_SERVER_LIBS ${CHANNEL_SERVER_LIBS} ${${_MODULE_PREFIX}_SERVER_LIBS})
endforeach()

add_library(${MODULE_NAME} ${CHANNEL_SERVER_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

target_link_libraries(${MODULE_NAME} ${CHANNEL_SERVER_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/Server")

