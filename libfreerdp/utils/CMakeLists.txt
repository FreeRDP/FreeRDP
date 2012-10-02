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
find_required_package(Threads)

set(FREERDP_UTILS_SRCS
	args.c
	dsp.c
	event.c
	error.c
	bitmap.c
	hexdump.c
	list.c
	file.c
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
	add_library(freerdp-utils OBJECT ${FREERDP_UTILS_SRCS})
else()
	add_library(freerdp-utils ${FREERDP_UTILS_SRCS})
endif()

set_target_properties(freerdp-utils PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(FREERDP_UTILS_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if(WIN32)
	set(FREERDP_UTILS_LIBS ${FREERDP_UTILS_LIBS} ws2_32)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	set(FREERDP_UTILS_LIBS ${FREERDP_UTILS_LIBS} rt)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(FREERDP_UTILS_LIBS ${FREERDP_UTILS_LIBS} winpr)
	set(FREERDP_LIBS ${FREERDP_LIBS} ${FREERDP_UTILS_LIBS} PARENT_SCOPE)
else()
	if(NOT WIN32)
		set(FREERDP_UTILS_LIBS ${FREERDP_UTILS_LIBS} winpr-crt)
	endif()
	set(FREERDP_UTILS_LIBS ${FREERDP_UTILS_LIBS} winpr-synch)
	target_link_libraries(freerdp-utils ${FREERDP_UTILS_LIBS})
	install(TARGETS freerdp-utils DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
