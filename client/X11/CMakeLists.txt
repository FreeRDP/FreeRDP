# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP X11 Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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
cmake_minimum_required(VERSION 3.13)

project(xfreerdp-client LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(CMAKE_COLOR_MAKEFILE "colorful CMake makefile" ON)
option(CMAKE_VERBOSE_MAKEFILE "verbose CMake makefile" ON)
option(CMAKE_POSITION_INDEPENDENT_CODE "build with position independent code (-fPIC or -fPIE)" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(PreventInSourceBuilds)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "project default" FORCE)
endif()

include(ConfigureFreeRDP)

include(GNUInstallDirsWrapper)

# RPATH configuration
include(ConfigureRPATH)

find_package(X11 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../resources)
include_directories(${X11_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

set(SRCS
	xf_utils.h
	xf_utils.c
	xf_gfx.c
	xf_gfx.h
	xf_rail.c
	xf_rail.h
	xf_input.c
	xf_input.h
	xf_event.c
	xf_event.h
	xf_floatbar.c
	xf_floatbar.h
	xf_input.c
	xf_input.h
	xf_channels.c
	xf_channels.h
	xf_cliprdr.c
	xf_cliprdr.h
	xf_monitor.c
	xf_monitor.h
	xf_disp.c
	xf_disp.h
	xf_graphics.c
	xf_graphics.h
	xf_keyboard.c
	xf_keyboard.h
	xf_video.c
	xf_video.h
	xf_window.c
	xf_window.h
	xf_client.c
	xf_client.h)

if (CHANNEL_TSMF_CLIENT)
	list(APPEND SRCS
		xf_tsmf.c
		xf_tsmf.h
	)
endif()

if(WITH_CLIENT_INTERFACE)
	if(CLIENT_INTERFACE_SHARED)
		add_library(${PROJECT_NAME} SHARED ${SRCS})
		if (WITH_LIBRARY_VERSIONING)
			set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
		endif()
	else()
		add_library(${PROJECT_NAME} ${SRCS})
	endif()
	target_include_directories(${PROJECT_NAME} INTERFACE $<INSTALL_INTERFACE:include>)

else()
	list(APPEND SRCS
		cli/xfreerdp.c xfreerdp.h
	)
	add_executable(${PROJECT_NAME} ${SRCS})
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "xfreerdp")
	include_directories(..)
endif()

set(LIBS
	${X11_LIBRARIES}
)

add_subdirectory(man)

find_package(X11 REQUIRED)
if(X11_XShm_FOUND)
	add_definitions(-DWITH_XSHM)
	include_directories(${X11_XShm_INCLUDE_PATH})
	list(APPEND LIBS
		${X11_Xext_LIB}
	)
endif()


option(WITH_XINERAMA "[X11] enable xinerama" ON)
if (WITH_XINERAMA)
	find_package(X11 REQUIRED)
	if(X11_Xinerama_FOUND)
		add_definitions(-DWITH_XINERAMA)
		include_directories(${X11_Xinerama_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xinerama_LIB}
		)
	endif()
endif()

option(WITH_XEXT "[X11] enable Xext" ON)
if (WITH_XEXT)
	find_package(X11 REQUIRED)
	if(X11_Xext_FOUND)
		add_definitions(-DWITH_XEXT)
		list(APPEND LIBS
			${X11_Xext_LIB}
		)
	endif()
endif()

option(WITH_XCURSOR "[X11] enalbe Xcursor" ON)
if (WITH_XCURSOR)
	find_package(X11 REQUIRED)
	if(X11_Xcursor_FOUND)
		add_definitions(-DWITH_XCURSOR)
		include_directories(${X11_Xcursor_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xcursor_LIB}
		)
	endif()
endif()

option(WITH_XV "[X11] enable Xv" ON)
if (WITH_XV)
	find_package(X11 REQUIRED)
	if(X11_Xv_FOUND)
		add_definitions(-DWITH_XV)
		include_directories(${X11_Xv_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xv_LIB}
		)
	endif()
endif()

option(WITH_XI "[X11] enalbe Xi" ON)
if (WITH_XI)
	find_package(X11 REQUIRED)
	if(X11_Xi_FOUND)
		add_definitions(-DWITH_XI)
		include_directories(${X11_Xi_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xi_LIB}
		)
	endif()
endif()

option(WITH_XRENDER "[X11] enable XRender" ON)
if(WITH_XRENDER)
	find_package(X11 REQUIRED)
	if(X11_Xrender_FOUND)
		add_definitions(-DWITH_XRENDER)
		include_directories(${X11_Xrender_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xrender_LIB}
		)
	endif()
endif()

option(WITH_XRANDR "[X11] enable XRandR" ON)
if (WITH_XRANDR)
	find_package(X11 REQUIRED)
	if(X11_Xrandr_FOUND)
		add_definitions(-DWITH_XRANDR)
		include_directories(${X11_Xrandr_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xrandr_LIB}
		)
	endif()
endif()

option(WITH_XFIXES "[X11] enable Xfixes" ON)
if (WITH_XFIXES)
	find_package(X11 REQUIRED)
	if(X11_Xfixes_FOUND)
		add_definitions(-DWITH_XFIXES)
		include_directories(${X11_Xfixes_INCLUDE_PATH})
		list(APPEND LIBS
			${X11_Xfixes_LIB}
		)
	endif()
endif()

include_directories(${PROJECT_SOURCE_DIR}/resources)

list(APPEND LIBS
	freerdp-client
	freerdp
	m
)
if (NOT APPLE)
	list(APPEND LIBS rt)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

if(WITH_IPP)
	target_link_libraries(${PROJECT_NAME} PRIVATE ${IPP_LIBRARY_LIST})
endif()

option(WITH_CLIENT_INTERFACE "Build clients as a library with an interface" OFF)
if(WITH_CLIENT_INTERFACE)
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
	add_subdirectory(cli)
else()
	install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/X11")

