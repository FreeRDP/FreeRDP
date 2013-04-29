# FreeRDP: A Remote Desktop Protocol Implementation
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
include_directories("../../winpr/tools/makecert")

set(${MODULE_PREFIX}_SRCS
	xf_peer.c
	xf_peer.h
	xf_input.c
	xf_input.h
	xf_encode.c
	xf_encode.h
	xf_interface.c
	xf_interface.h
	xfreerdp.c
	xfreerdp.h)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(XEXT_FEATURE_TYPE "RECOMMENDED")
set(XEXT_FEATURE_PURPOSE "X11 extension")
set(XEXT_FEATURE_DESCRIPTION "X11 core extensions")

set(XSHM_FEATURE_TYPE "RECOMMENDED")
set(XSHM_FEATURE_PURPOSE "X11 shared memory")
set(XSHM_FEATURE_DESCRIPTION "X11 shared memory extension")

set(XTEST_FEATURE_TYPE "RECOMMENDED")
set(XTEST_FEATURE_PURPOSE "X11 input event injection")
set(XTEST_FEATURE_DESCRIPTION "X11 input event injection extension")

set(XFIXES_FEATURE_TYPE "RECOMMENDED")
set(XFIXES_FEATURE_PURPOSE "X11 region")
set(XFIXES_FEATURE_DESCRIPTION "X11 region fix extension")

set(XDAMAGE_FEATURE_TYPE "RECOMMENDED")
set(XDAMAGE_FEATURE_PURPOSE "X11 region damage")
set(XDAMAGE_FEATURE_DESCRIPTION "X11 region damage extension")

find_feature(Xext ${XEXT_FEATURE_TYPE} ${XEXT_FEATURE_PURPOSE} ${XEXT_FEATURE_DESCRIPTION})
find_feature(XShm ${XSHM_FEATURE_TYPE} ${XSHM_FEATURE_PURPOSE} ${XSHM_FEATURE_DESCRIPTION})
find_feature(XTest ${XTEST_FEATURE_TYPE} ${XTEST_FEATURE_PURPOSE} ${XTEST_FEATURE_DESCRIPTION})
find_feature(Xfixes ${XFIXES_FEATURE_TYPE} ${XFIXES_FEATURE_PURPOSE} ${XFIXES_FEATURE_DESCRIPTION})
find_feature(Xdamage ${XDAMAGE_FEATURE_TYPE} ${XDAMAGE_FEATURE_PURPOSE} ${XDAMAGE_FEATURE_DESCRIPTION})

if(WITH_XSHM)
	add_definitions(-DWITH_XSHM)
	include_directories(${XSHM_INCLUDE_DIRS})
endif()

if(WITH_XEXT)
	add_definitions(-DWITH_XEXT)
	include_directories(${XEXT_INCLUDE_DIRS})
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${XEXT_LIBRARIES})
endif()

if(WITH_XDAMAGE)
        add_definitions(-DWITH_XDAMAGE)
        include_directories(${XDAMAGE_INCLUDE_DIRS})
        set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${XDAMAGE_LIBRARIES})
endif()

if(WITH_XFIXES)
        add_definitions(-DWITH_XFIXES)
        include_directories(${XFIXES_INCLUDE_DIRS})
        target_link_libraries(${MODULE_NAME} ${XFIXES_LIBRARIES})
        set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${XFIXES_LIBRARIES})
endif()

if(WITH_XTEST)
        add_definitions(-DWITH_XTEST)
        include_directories(${XTEST_INCLUDE_DIRS})
        set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${XTEST_LIBRARIES})
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${X11_LIBRARIES})

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-common freerdp-codec freerdp-primitives freerdp-utils freerdp-gdi freerdp-crypto freerdp-locale)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-sspi)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr-makecert-tool)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/X11")

