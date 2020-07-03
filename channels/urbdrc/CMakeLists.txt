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

include_directories(common)
add_subdirectory(common)

if(WITH_CLIENT_CHANNELS)
    option(WITH_DEBUG_URBDRC "Dump data send/received in URBDRC channel" OFF)

    find_package(libusb-1.0 REQUIRED)
    include_directories(${LIBUSB_1_INCLUDE_DIRS})

    add_channel_client(${MODULE_PREFIX} ${CHANNEL_NAME})
endif()
