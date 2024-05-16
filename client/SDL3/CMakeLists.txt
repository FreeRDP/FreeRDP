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

if(POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()
if (NOT FREERDP_DEFAULT_PROJECT_VERSION)
	set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(sdl3-freerdp
	LANGUAGES CXX
	VERSION ${FREERDP_DEFAULT_PROJECT_VERSION}
)

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(CommonConfigOptions)

include(ConfigureFreeRDP)

option(WITH_DEBUG_SDL_EVENTS "[dangerous, not for release builds!] Debug SDL events" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SDL_KBD_EVENTS "[dangerous, not for release builds!] Debug SDL keyboard events" ${DEFAULT_DEBUG_OPTION})
option(WITH_WIN_CONSOLE "Build ${PROJECT_NAME} with console support" ON)
option(WITH_SDL_LINK_SHARED "link SDL dynamic or static" ON)

if(WITH_WIN_CONSOLE)
  set(WIN32_GUI_FLAG "TRUE")
else()
  set(WIN32_GUI_FLAG "WIN32")
endif()


if (WITH_DEBUG_SDL_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_EVENTS)
endif()
if (WITH_DEBUG_SDL_KBD_EVENTS)
	add_definitions(-DWITH_DEBUG_SDL_KBD_EVENTS)
endif()

find_package(SDL3 REQUIRED COMPONENTS)

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
	sdl_clip.hpp
	sdl_clip.cpp
)

add_library(sdl3-prefs STATIC
    sdl_prefs.hpp
    sdl_prefs.cpp
)

add_subdirectory(aad)
list(APPEND LIBS
        winpr
        freerdp
        freerdp-client
        Threads::Threads
        sdl3_client_res
        sdl3-dialogs
        sdl3-aad-view
        sdl3-prefs
    )

if (NOT WITH_SDL_LINK_SHARED)
	list(APPEND LIBS ${SDL3_STATIC_LIBRARIES})
else()
	list(APPEND LIBS ${SDL3_LIBRARIES})
endif()

AddTargetWithResourceFile(${PROJECT_NAME} "${WIN32_GUI_FLAG}" "${PROJECT_VERSION}" SRCS)

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_link_libraries(sdl3-prefs winpr)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/SDL")
if (NOT WITH_CLIENT_SDL_VERSIONED)
	set_target_properties(${PROJECT_NAME}
		PROPERTIES
		OUTPUT_NAME "sdl-freerdp"
	)
endif()
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

add_subdirectory(man)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
