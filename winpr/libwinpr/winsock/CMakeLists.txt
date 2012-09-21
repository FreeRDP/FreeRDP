# WinPR: Windows Portable Runtime
# libwinpr-winsock cmake build script
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

set(WINPR_WINSOCK_SRCS
	winsock.c)

if(WITH_MONOLITHIC_BUILD)
	add_library(winpr-winsock OBJECT ${WINPR_WINSOCK_SRCS})
else()
	add_library(winpr-winsock ${WINPR_WINSOCK_SRCS})
endif()

set_target_properties(winpr-winsock PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

if(WIN32)
	set(WINPR_WINSOCK_LIBS ${WINPR_WINSOCK_LIBS} ws2_32)
endif()

if(WITH_MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${WINPR_WINSOCK_LIBS} PARENT_SCOPE)
else()
	target_link_libraries(winpr-winsock ${WINPR_WINSOCK_LIBS})
	install(TARGETS winpr-winsock DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
