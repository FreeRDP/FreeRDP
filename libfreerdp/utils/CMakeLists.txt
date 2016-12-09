# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-utils cmake build script
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

set(MODULE_NAME "freerdp-utils")
set(MODULE_PREFIX "FREERDP_UTILS")

set(${MODULE_PREFIX}_SRCS
	passphrase.c
	pcap.c
	profiler.c
	ringbuffer.c
	signal.c
	stopwatch.c)

if(NOT WIN32)
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} msusb.c)
endif()

freerdp_module_add(${${MODULE_PREFIX}_SRCS})

freerdp_library_add(${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
	freerdp_library_add(ws2_32)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	freerdp_library_add(rt)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
