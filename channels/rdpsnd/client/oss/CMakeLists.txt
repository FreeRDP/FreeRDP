# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright (c) 2015 Rozhuk Ivan <rozhuk.im@gmail.com>
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

define_channel_client_subsystem("rdpsnd" "oss" "")

find_package(OSS REQUIRED)

set(${MODULE_PREFIX}_SRCS rdpsnd_oss.c)

set(${MODULE_PREFIX}_LIBS winpr freerdp ${OSS_LIBRARIES})

include_directories(..)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(SYSTEM ${OSS_INCLUDE_DIRS})
cleaning_configure_file(${CMAKE_SOURCE_DIR}/cmake/oss-includes.h.in ${CMAKE_CURRENT_BINARY_DIR}/oss-includes.h @ONLY)

add_channel_client_subsystem_library(${MODULE_PREFIX} ${MODULE_NAME} ${CHANNEL_NAME} "" TRUE "")
