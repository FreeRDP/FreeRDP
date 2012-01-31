# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP X11 Server cmake build script
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

include_directories(${X11_INCLUDE_DIRS})

add_executable(xfreerdp-server
	xf_peer.c
	xf_event.c
	xf_input.c
	xf_encode.c
	xfreerdp.c)

find_suggested_package(XShm)
if(WITH_XSHM)
	add_definitions(-DWITH_XSHM)
	include_directories(${XSHM_INCLUDE_DIRS})
endif()

find_suggested_package(Xext)
if(WITH_XEXT)
	add_definitions(-DWITH_XEXT)
	include_directories(${XEXT_INCLUDE_DIRS})
	target_link_libraries(xfreerdp-server ${XEXT_LIBRARIES})
endif()

find_suggested_package(Xdamage)
if(WITH_XDAMAGE)
        add_definitions(-DWITH_XDAMAGE)
        include_directories(${XDAMAGE_INCLUDE_DIRS})
        target_link_libraries(xfreerdp-server ${XDAMAGE_LIBRARIES})
endif()

find_suggested_package(Xfixes)
if(WITH_XFIXES)
        add_definitions(-DWITH_XFIXES)
        include_directories(${XFIXES_INCLUDE_DIRS})
        target_link_libraries(xfreerdp-server ${XFIXES_LIBRARIES})
endif()

find_suggested_package(XTest)
if(WITH_XTEST)
        add_definitions(-DWITH_XTEST)
        include_directories(${XTEST_INCLUDE_DIRS})
        target_link_libraries(xfreerdp-server ${XTEST_LIBRARIES})
endif()

target_link_libraries(xfreerdp-server freerdp-core)
target_link_libraries(xfreerdp-server freerdp-codec)
target_link_libraries(xfreerdp-server freerdp-utils)
target_link_libraries(xfreerdp-server freerdp-gdi)
target_link_libraries(xfreerdp-server freerdp-kbd)
target_link_libraries(xfreerdp-server ${X11_LIBRARIES})

