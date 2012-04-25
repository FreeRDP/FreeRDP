# FreeRDP: A Remote Desktop Protocol Client
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

set(LIBUSB_UDEVMAN_SRCS
	libusb_udevman.c
	libusb_udevice.c
	libusb_udevice.h
	request_queue.c
	request_queue.h
)

include_directories(..)

add_library(libusb_udevman ${LIBUSB_UDEVMAN_SRCS})
set_target_properties(libusb_udevman PROPERTIES PREFIX "")

target_link_libraries(libusb_udevman udev)  
target_link_libraries(libusb_udevman dbus-glib-1)  
target_link_libraries(libusb_udevman pthread) 
target_link_libraries(libusb_udevman usb-1.0)  
target_link_libraries(libusb_udevman freerdp-utils)

install(TARGETS libusb_udevman DESTINATION ${FREERDP_PLUGIN_PATH})
