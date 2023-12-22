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

if (NOT FREERDP_DEFAULT_PROJECT_VERSION)
	set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(sdl-freerdp
	LANGUAGES CXX
	VERSION ${FREERDP_DEFAULT_PROJECT_VERSION}
)

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(CMAKE_COLOR_MAKEFILE "colorful CMake makefile" ON)
option(CMAKE_VERBOSE_MAKEFILE "verbose CMake makefile" ON)
option(CMAKE_POSITION_INDEPENDENT_CODE "build with position independent code (-fPIC or -fPIE)" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(PreventInSourceBuilds)

include(ConfigureFreeRDP)

include(GNUInstallDirsWrapper)

# RPATH configuration
include(ConfigureRPATH)

option(WITH_DEBUG_SDL_EVENTS "[dangerous, not for release builds!] Debug SDL events" OFF)
option(WITH_DEBUG_SDL_KBD_EVENTS "[dangerous, not for release builds!] Debug SDL keyboard events" OFF)
option(WITH_WIN_CONSOLE "Build ${PROJECT_NAME} with console support" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "project default" FORCE)
endif()

# Configure MSVC Runtime
include(MSVCRuntime)

if(WITH_WIN_CONSOLE)
  set(WIN32_GUI_FLAG "")
else()
  set(WIN32_GUI_FLAG "WIN32")
endif()


if (WITH_DEBUG_SDL_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_EVENTS)
endif()
if (WITH_DEBUG_SDL_KBD_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_KBD_EVENTS)
endif()

find_package(SDL2 REQUIRED COMPONENTS)
include_directories(${SDL2_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})

find_package(Threads REQUIRED)

add_subdirectory(dialogs)
set(SRCS
    sdl_types.hpp
    sdl_utils.cpp
    sdl_utils.hpp
    sdl_kbd.cpp
    sdl_kbd.hpp
    sdl_touch.cpp
    sdl_touch.hpp
    sdl_pointer.cpp
    sdl_pointer.hpp
    sdl_disp.cpp
    sdl_disp.hpp
    sdl_monitor.cpp
    sdl_monitor.hpp
    sdl_freerdp.hpp
    sdl_freerdp.cpp
    sdl_channels.hpp
    sdl_channels.cpp
    sdl_window.hpp
    sdl_window.cpp
)

add_subdirectory(aad)
set(LIBS
	${SDL2_LIBRARIES}
        winpr
        freerdp
        freerdp-client
        Threads::Threads
        sdl_client_res
        dialogs
        aad-view
    )

add_executable(${PROJECT_NAME}
	${WIN32_GUI_FLAG}
	${SRCS}
    )

if (WITH_BINARY_VERSIONING)
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}${PROJECT_VERSION_MAJOR}")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/SDL")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

add_subdirectory(man)
