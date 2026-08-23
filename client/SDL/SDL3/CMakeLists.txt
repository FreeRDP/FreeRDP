# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2022 Armin Novak <anovak@thincast.com>
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
set(MODULE_NAME "sdl3-freerdp")

find_package(SDL3 REQUIRED)

find_package(Threads REQUIRED)

add_subdirectory(dialogs)
set(SRCS
    sdl_types.hpp
    sdl_utils.cpp
    sdl_utils.hpp
    sdl_input.cpp
    sdl_input.hpp
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
    sdl_context.hpp
    sdl_context.cpp
    sdl_rail.hpp
    sdl_rail.cpp
    sdl_rail_window.hpp
    sdl_rail_window.cpp
    sdl_wayland.hpp
    sdl_wayland.cpp
    sdl_x11.hpp
    sdl_x11.cpp
)

# Native X11 helpers for RAIL (_NET_WM_MOVERESIZE, compositor detection); no-op stubs without libX11.
set(SDL_X11_NATIVE OFF)
if(NOT WIN32 AND NOT APPLE)
  find_package(X11 QUIET)
  if(X11_FOUND)
    set(SDL_X11_NATIVE ON)
  endif()
endif()

# Native Wayland helpers for RAIL (xdg_toplevel.move/resize); no-op stubs without wayland-client.
set(SDL_WAYLAND_NATIVE OFF)
if(NOT WIN32 AND NOT APPLE)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
    # Host scanner and canonical protocol, so a cross build does not pick the target ones.
    pkg_get_variable(WAYLAND_SCANNER_BIN wayland-scanner wayland_scanner)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
  endif()
  if(NOT WAYLAND_SCANNER_BIN)
    find_program(WAYLAND_SCANNER_BIN wayland-scanner)
  endif()
  # Prefer wayland-protocols; fall back to the copy bundled with uwac.
  if(WAYLAND_PROTOCOLS_DIR AND EXISTS ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
    set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
  else()
    set(XDG_SHELL_XML ${CMAKE_SOURCE_DIR}/uwac/protocols/xdg-shell.xml)
  endif()
  if(WAYLAND_CLIENT_FOUND AND WAYLAND_SCANNER_BIN AND EXISTS ${XDG_SHELL_XML})
    set(SDL_WAYLAND_NATIVE ON)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
      COMMAND ${WAYLAND_SCANNER_BIN} client-header ${XDG_SHELL_XML}
              ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h DEPENDS ${XDG_SHELL_XML}
    )
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
      COMMAND ${WAYLAND_SCANNER_BIN} private-code ${XDG_SHELL_XML} ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
      DEPENDS ${XDG_SHELL_XML}
    )
    list(APPEND SRCS ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
         ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
    )
  endif()
endif()

add_win_console_manifest(SRCS)

list(
  APPEND
  LIBS
  winpr
  freerdp
  freerdp-client
  Threads::Threads
  sdl3_client_res
  sdl3-dialogs
  sdl-common-aad-view
  sdl-common-prefs
)

if(NOT WITH_SDL_LINK_SHARED)
  list(APPEND LIBS SDL3::SDL3-static)
else()
  list(APPEND LIBS SDL3::SDL3)
endif()
set_target_properties(SDL3::Headers PROPERTIES SYSTEM TRUE)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

addtargetwithresourcefile(${MODULE_NAME} "${WIN32_GUI_FLAG}" "${PROJECT_VERSION}" SRCS)

if(SDL_WAYLAND_NATIVE)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_SDL_WAYLAND_NATIVE)
  target_include_directories(${MODULE_NAME} PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
  list(APPEND LIBS ${WAYLAND_CLIENT_LIBRARIES})
endif()

if(SDL_X11_NATIVE)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_SDL_X11_NATIVE)
  target_include_directories(${MODULE_NAME} PRIVATE ${X11_INCLUDE_DIR})
  list(APPEND LIBS ${X11_LIBRARIES})
endif()

target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/SDL")
get_target_property(SDL_CLIENT_BINARY_NAME ${MODULE_NAME} OUTPUT_NAME)
if(NOT WITH_CLIENT_SDL_VERSIONED)
  string(REPLACE "${MODULE_NAME}" "${PROJECT_NAME}" SDL_CLIENT_BINARY_NAME "${SDL_CLIENT_BINARY_NAME}")
  set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${SDL_CLIENT_BINARY_NAME})
endif()

string(TIMESTAMP SDL_CLIENT_YEAR "%Y")
set(SDL_CLIENT_UUID "com.freerdp.client.sdl3")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sdl_config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/sdl_config.hpp @ONLY)

installwithrpath(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
install_freerdp_desktop("${MODULE_NAME}" "${SDL_CLIENT_UUID}")

add_subdirectory(man)
