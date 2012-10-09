# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP X11 Server cmake build script
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

set(MODULE_NAME "xfreerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER_X11")

include_directories(${X11_INCLUDE_DIRS})

set(${MODULE_PREFIX}_SRCS
	xf_peer.c
	xf_event.c
	xf_input.c
	xf_encode.c
	xfreerdp.c)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

find_suggested_package(XShm)
if(WITH_XSHM)
	add_definitions(-DWITH_XSHM)
	include_directories(${XSHM_INCLUDE_DIRS})
endif()

find_suggested_package(Xext)
if(WITH_XEXT)
	add_definitions(-DWITH_XEXT)
	include_directories(${XEXT_INCLUDE_DIRS})
	target_link_libraries(${MODULE_NAME} ${XEXT_LIBRARIES})
endif()

find_suggested_package(Xdamage)
if(WITH_XDAMAGE)
        add_definitions(-DWITH_XDAMAGE)
        include_directories(${XDAMAGE_INCLUDE_DIRS})
        target_link_libraries(${MODULE_NAME} ${XDAMAGE_LIBRARIES})
endif()

find_suggested_package(Xfixes)
if(WITH_XFIXES)
        add_definitions(-DWITH_XFIXES)
        include_directories(${XFIXES_INCLUDE_DIRS})
        target_link_libraries(${MODULE_NAME} ${XFIXES_LIBRARIES})
endif()

find_suggested_package(XTest)
if(WITH_XTEST)
        add_definitions(-DWITH_XTEST)
        include_directories(${XTEST_INCLUDE_DIRS})
        target_link_libraries(${MODULE_NAME} ${XTEST_LIBRARIES})
endif()

if(WITH_MONOLITHIC_BUILD)
	target_link_libraries(${MODULE_NAME} freerdp)
	target_link_libraries(${MODULE_NAME} winpr)
else()
	target_link_libraries(${MODULE_NAME}
		freerdp-core
		freerdp-codec
		freerdp-utils
		freerdp-gdi
		freerdp-crypto
		freerdp-locale)

	target_link_libraries(${MODULE_NAME} winpr-sspi)
endif()

target_link_libraries(${MODULE_NAME} ${X11_LIBRARIES})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/X11")
