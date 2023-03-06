# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2022 Armin Novak <anovak@thincast.com>
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

project(sdl-client CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
list(APPEND CMAKE_MODULE_PATH ../../cmake/)

include(GNUInstallDirsWrapper)
# RPATH configuration
 set(CMAKE_SKIP_BUILD_RPATH FALSE)
 set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
 if (APPLE)
   set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
   set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
 else (APPLE)
   set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
   if (NOT FREEBSD)
     set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:\$ORIGIN/..")
     option(WITH_ADD_PLUGIN_TO_RPATH "Add extension and plugin path to RPATH" OFF)
     if (WITH_ADD_PLUGIN_TO_RPATH)
       set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${FREERDP_EXTENSION_REL_PATH}:\$ORIGIN/../${FREERDP_PLUGIN_PATH}:${CMAKE_INSTALL_RPATH}")
     endif()
   endif()
 endif(APPLE)

option(WITH_DEBUG_SDL_EVENTS "[dangerous, not for release builds!] Debug SDL events" OFF)
option(WITH_DEBUG_SDL_KBD_EVENTS "[dangerous, not for release builds!] Debug SDL keyboard events" OFF)

if (WITH_DEBUG_SDL_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_EVENTS)
endif()
if (WITH_DEBUG_SDL_KBD_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_KBD_EVENTS)
endif()

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})

# Here we add dependencies for stand alone builds.
# Only add these if not build within FreeRDP source tree
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	find_package(WinPR 3 REQUIRED)
	include_directories(${WinPR_INCLUDE_DIR})

	find_package(FreeRDP 3 REQUIRED)
	include_directories(${FreeRDP_INCLUDE_DIR})

	find_package(FreeRDP-Client 3 REQUIRED)
	include_directories(${FreeRDP-Client_INCLUDE_DIR})
endif()

set(SRCS
    sdl_utils.cpp
    sdl_utils.h
    sdl_kbd.cpp
    sdl_kbd.h
    sdl_touch.cpp
    sdl_touch.h
    sdl_pointer.cpp
    sdl_pointer.h
    sdl_disp.cpp
    sdl_disp.h
    sdl_monitor.cpp
    sdl_monitor.h
    sdl_freerdp.h
    sdl_freerdp.cpp
    sdl_channels.h
    sdl_channels.cpp)

set(LIBS
	${SDL2_LIBRARIES}
        winpr
        freerdp
        freerdp-client
    )

add_executable(${PROJECT_NAME}
	${SRCS}
    )

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "sdl-freerdp")
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/SDL")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
