# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Bernhard Miklautz <bmiklautz@thinstuff.at>
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

set(MODULE_NAME "freerdp-android")
set(MODULE_PREFIX "FREERDP_CLIENT_ANDROID")


include_directories(.)
include_directories(generated)

if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-sign")
endif()

set(${MODULE_PREFIX}_SRCS
	android_debug.h
	android_event.c
	android_event.h
	android_freerdp.c
	android_freerdp.h
	android_jni_utils.c
	android_jni_utils.h
	android_jni_callback.c
	android_jni_callback.h)

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS}
	generated/android_freerdp_jni.c
	generated/android_freerdp_jni.h)

if(WITH_CLIENT_CHANNELS)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS}
		android_cliprdr.c
		android_cliprdr.h)
endif()

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-gdi freerdp-utils)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-crt winpr-synch)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} dl)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} log)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} jnigraphics)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME}
	PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ANDROID_BINARY_DIR}/libs/${ANDROID_ABI}")

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Android")
