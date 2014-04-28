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

define_channel("urbdrc")

if(NOT WIN32)
	find_package(UDev)
	find_package(UUID)
	find_package(DbusGlib)
	find_package(libusb-1.0)
endif()

if(UDEV_FOUND AND UUID_FOUND AND DBUS_GLIB_FOUND AND LIBUSB_1_FOUND)
	set(URBDRC_DEPENDENCIES_FOUND TRUE)
	message(STATUS "Found all URBDRC dependencies")
endif()

if(WITH_CLIENT_CHANNELS)
	if(URBDRC_DEPENDENCIES_FOUND)
		add_channel_client(${MODULE_PREFIX} ${CHANNEL_NAME})
	endif()
endif()
