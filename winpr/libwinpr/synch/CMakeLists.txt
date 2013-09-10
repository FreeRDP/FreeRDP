# WinPR: Windows Portable Runtime
# libwinpr-synch cmake build script
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

set(MODULE_NAME "winpr-synch")
set(MODULE_PREFIX "WINPR_SYNCH")

INCLUDE (CheckLibraryExists) 
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_library_exists(pthread pthread_tryjoin_np "" HAVE_PTHREAD_GNU_EXT)
list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

if(HAVE_PTHREAD_GNU_EXT)
	add_definitions(-D_GNU_SOURCE -DHAVE_PTHREAD_GNU_EXT)
endif(HAVE_PTHREAD_GNU_EXT)
	
include_directories(../thread)

set(${MODULE_PREFIX}_SRCS
	address.c
	barrier.c
	condition.c
	critical.c
	event.c
	init.c
	mutex.c
	semaphore.c
	sleep.c
	srw.c
	synch.c
	synch.h
	timer.c
	wait.c)

add_complex_library(MODULE ${MODULE_NAME} TYPE "OBJECT"
	MONOLITHIC ${MONOLITHIC_BUILD}
	SOURCES ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if((NOT WIN32) AND (NOT APPLE) AND (NOT ANDROID))
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} rt)
endif()

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD} INTERNAL
	MODULE winpr
	MODULES winpr-handle winpr-interlocked winpr-thread)

if(MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
