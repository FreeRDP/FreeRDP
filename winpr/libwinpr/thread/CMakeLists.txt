# WinPR: Windows Portable Runtime
# libwinpr-thread cmake build script
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

set(CMAKE_THREAD_PREFER_PTHREAD)
find_required_package(Threads)

set(WINPR_THREAD_SRCS
	thread.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-thread OBJECT ${WINPR_THREAD_SRCS})
else()
	add_library(winpr-thread ${WINPR_THREAD_SRCS})
endif()

set_target_properties(winpr-thread PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

set(WINPR_SYNCH_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	set(WINPR_SYNCH_LIBS ${WINPR_SYNCH_LIBS} rt)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${WINPR_SYNCH_LIBS} PARENT_SCOPE)
else()
	if(NOT WIN32)
		set(WINPR_SYNCH_LIBS ${WINPR_SYNCH_LIBS} winpr-handle)
	endif()

	target_link_libraries(winpr-thread ${WINPR_SYNCH_LIBS})
	install(TARGETS winpr-thread DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

