# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

define_channel_client_subsystem("rdpsnd" "ios" "")

FIND_LIBRARY(CORE_AUDIO CoreAudio)
FIND_LIBRARY(AUDIO_TOOL AudioToolbox)
FIND_LIBRARY(CORE_FOUNDATION CoreFoundation)

set(${MODULE_PREFIX}_SRCS
	rdpsnd_ios.c
	TPCircularBuffer.c)

include_directories(..)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-utils)
	
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} 
    ${AUDIO_TOOL}
    ${CORE_AUDIO}
    ${CORE_FOUNDATION})

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if(NOT STATIC_CHANNELS)
	install(TARGETS ${MODULE_NAME} DESTINATION ${FREERDP_ADDIN_PATH})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Client/ios")
