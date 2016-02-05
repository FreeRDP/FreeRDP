# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2012 Atrust corp.
# Copyright 2012 Alfred Liu <alfred.liu@atruscorp.com>
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

define_channel_client("urbdrc")

set(${MODULE_PREFIX}_SRCS
	searchman.c
	searchman.h
	isoch_queue.c
	isoch_queue.h
	data_transfer.c
	data_transfer.h
	urbdrc_main.c
	urbdrc_main.h
	urbdrc_types.h)

include_directories(..)

add_channel_client_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} TRUE "DVCPluginEntry")



#set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} libusb-devman)

set(${MODULE_PREFIX}_LIBS
    ${DBUS_GLIB_LIBRARIES}
    ${UUID_LIBRARIES})
if (UDEV_FOUND AND UDEV_LIBRARIES)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${UDEV_LIBRARIES})
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if (WITH_DEBUG_SYMBOLS AND MSVC AND NOT STATIC_CHANNELS AND BUILD_SHARED_LIBS)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${FREERDP_ADDIN_PATH} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Channels/${CHANNEL_NAME}/Client")

# libusb subsystem
add_channel_client_subsystem(${MODULE_PREFIX} ${CHANNEL_NAME} "libusb" "")
