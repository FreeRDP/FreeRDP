# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2024 Armin Novak <anovak@thincast.com>
# Copyright 2024 Thincast Technologies GmbH
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
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(sdl-freerdp LANGUAGES CXX VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(CommonConfigOptions)

include(ConfigureFreeRDP)
include(CXXCompilerFlags)

option(WITH_DEBUG_SDL_EVENTS "[dangerous, not for release builds!] Debug SDL events" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SDL_KBD_EVENTS "[dangerous, not for release builds!] Debug SDL keyboard events"
       ${DEFAULT_DEBUG_OPTION}
)
option(WITH_WIN_CONSOLE "Build ${PROJECT_NAME} with console support" ON)
option(WITH_SDL_LINK_SHARED "link SDL dynamic or static" ON)

if(WITH_WIN_CONSOLE)
  set(WIN32_GUI_FLAG "TRUE")
else()
  set(WIN32_GUI_FLAG "WIN32")
endif()

if(WITH_DEBUG_SDL_EVENTS)
  add_compile_definitions(WITH_DEBUG_SDL_EVENTS)
endif()
if(WITH_DEBUG_SDL_KBD_EVENTS)
  add_compile_definitions(WITH_DEBUG_SDL_KBD_EVENTS)
endif()

include(CMakeDependentOption)

find_package(SDL2)
find_package(SDL3)

cmake_dependent_option(WITH_CLIENT_SDL_VERSIONED "append sdl version to client binaries" OFF WITH_CLIENT_SDL OFF)
cmake_dependent_option(
  WITH_CLIENT_SDL2 "[experimental] build experimental SDL2 client" ${SDL2_FOUND} WITH_CLIENT_SDL OFF
)
cmake_dependent_option(
  WITH_CLIENT_SDL3 "[experimental] build experimental SDL3 client" ${SDL3_FOUND} WITH_CLIENT_SDL OFF
)

if(WITH_CLIENT_SDL2 AND WITH_CLIENT_SDL3)
  message("Building both, SDL2 and SDL3 clients, forcing WITH_CLIENT_SDL_VERSIONED=ON")
  set(WITH_CLIENT_SDL_VERSIONED ON)
endif()

if(NOT SDL2_FOUND AND NOT SDL3_FOUND)
  message(FATAL_ERROR "No SDL library detected, giving up. Install SDL2 or SDL3 development package to fix")
endif()

add_subdirectory(common)
include_directories(common)

if(WITH_CLIENT_SDL2)
  if(SDL2_FOUND)
    add_subdirectory(SDL2)
  else()
    message(WARNING "SDL2 requested but not found, continuing build without SDL2 client")
  endif()
endif()

if(WITH_CLIENT_SDL3)
  if(SDL3_FOUND)
    add_subdirectory(SDL3)
  else()
    message(WARNING "SDL3 requested but not found, continuing build without SDL3 client")
  endif()
endif()
