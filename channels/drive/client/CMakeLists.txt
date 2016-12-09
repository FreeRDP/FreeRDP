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

define_channel_client("drive")

set(${MODULE_PREFIX}_SRCS
	drive_file.c
	drive_file.h
	drive_main.c)

if(WIN32)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS}
		statvfs.c
		statvfs.h
		dirent.h)
endif()

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DeviceServiceEntry")



target_link_libraries(${MODULE_NAME} winpr freerdp)


if (WITH_DEBUG_SYMBOLS AND MSVC AND NOT BUILTIN_CHANNELS AND BUILD_SHARED_LIBS)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${FREERDP_ADDIN_PATH} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Client")
