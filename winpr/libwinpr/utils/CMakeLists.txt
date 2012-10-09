# WinPR: Windows Portable Runtime
# libwinpr-utils cmake build script
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

set(MODULE_NAME "winpr-utils")
set(MODULE_PREFIX "WINPR_UTILS")

set(${MODULE_PREFIX}_SRCS
	ntlm.c
	print.c
	sam.c
	stream.c)

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

if(MONOLITHIC_BUILD)
	add_library(${MODULE_NAME} OBJECT ${${MODULE_PREFIX}_SRCS})
else()
	add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
endif()

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${ZLIB_LIBRARIES}
	${OPENSSL_LIBRARIES})

if(MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()
	if (NOT WIN32)
		set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr-crt)
	endif()
	
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR")
