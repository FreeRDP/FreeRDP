# FreeRDP: A Remote Desktop Protocol Client
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

set(CMAKE_THREAD_PREFER_PTHREAD)
find_required_package(Threads)

set(${MODULE_PREFIX}_SRCS
	args.c
	dsp.c
	event.c
	bitmap.c
	hexdump.c
	list.c
	file.c
	msusb.c
	load_plugin.c
	memory.c
	passphrase.c
	pcap.c
	profiler.c
	rail.c
	signal.c
	sleep.c
	stopwatch.c
	stream.c
	string.c
	svc_plugin.c
	tcp.c
	thread.c
	time.c
	uds.c
	unicode.c
	wait_obj.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(${MODULE_NAME} OBJECT ${${MODULE_PREFIX}_SRCS})
else()
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if(WIN32)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ws2_32)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} rt)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	if(NOT WIN32)
		set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr-crt)
	endif()

	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr-synch)

	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP")
