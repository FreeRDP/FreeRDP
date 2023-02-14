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

option(CMAKE_COLOR_MAKEFILE "colorful CMake makefile" ON)
option(CMAKE_VERBOSE_MAKEFILE "verbose CMake makefile" ON)
option(CMAKE_POSITION_INDEPENDENT_CODE "build with position independent code (-fPIC or -fPIE)" ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)

include(ConfigureFreeRDP)

include(GNUInstallDirsWrapper)

# RPATH configuration
include(ConfigureRPATH)

option(WITH_DEBUG_SDL_EVENTS "[dangerous, not for release builds!] Debug SDL events" OFF)
option(WITH_DEBUG_SDL_KBD_EVENTS "[dangerous, not for release builds!] Debug SDL keyboard events" OFF)

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
    sdl_channels.cpp)

set(LIBS
	${SDL2_LIBRARIES}
        winpr
        freerdp
        freerdp-client
        Threads::Threads
        dialogs
    )

option(WITH_WEBVIEW "Build with QtWebEngine support for AAD login popup browser" OFF)
if (WITH_WEBVIEW)
  find_package(Qt5 COMPONENTS WebEngineWidgets REQUIRED)

  list(APPEND SRCS
      sdl_webview.cpp
      sdl_webview.hpp
      )
  list(APPEND LIBS Qt5::WebEngineWidgets)
endif()

add_executable(${PROJECT_NAME}
	${SRCS}
    )

if (WITH_WEBVIEW)
	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		-DWITH_WEBVIEW
	)
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    OUTPUT_NAME "sdl-freerdp"
    )
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/SDL")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
