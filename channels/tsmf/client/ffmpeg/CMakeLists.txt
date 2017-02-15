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

define_channel_client_subsystem("tsmf" "ffmpeg" "decoder")

set(${MODULE_PREFIX}_SRCS
	tsmf_ffmpeg.c)

include_directories(..)
include_directories(${FFMPEG_INCLUDE_DIRS})

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")



if(APPLE)
    # For this to work on apple, we need to add some frameworks
    FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
    FIND_LIBRARY(COREVIDEO_LIBRARY CoreVideo)
    FIND_LIBRARY(COREVIDEODECODE_LIBRARY VideoDecodeAcceleration)
    set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${FFMPEG_LIBRARIES} ${COREFOUNDATION_LIBRARY} ${COREVIDEO_LIBRARY} ${COREVIDEODECODE_LIBRARY})
    target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} freerdp)
else()
    set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${FFMPEG_LIBRARIES})
    target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
endif()

if (WITH_DEBUG_SYMBOLS AND MSVC AND NOT BUILTIN_CHANNELS AND BUILD_SHARED_LIBS)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${FREERDP_ADDIN_PATH} COMPONENT symbols)
endif()

