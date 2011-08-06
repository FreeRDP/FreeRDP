# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-utils cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(CMAKE_THREAD_PREFER_PTHREAD)
find_package(Threads REQUIRED)

set(FREERDP_UTILS_SRCS
	args.c
	blob.c
	event.c
	hexdump.c
	list.c
	load_plugin.c
	memory.c
	mutex.c
	semaphore.c
	stream.c
	svc_plugin.c
	thread.c
	unicode.c
	wait_obj.c)

add_definitions(-DPLUGIN_PATH="${FREERDP_PLUGIN_PATH}")

add_library(freerdp-utils SHARED ${FREERDP_UTILS_SRCS})

set_target_properties(freerdp-utils PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION})

target_link_libraries(freerdp-utils ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS freerdp-utils DESTINATION lib)
