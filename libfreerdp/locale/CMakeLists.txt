# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-locale cmake build script
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

set(MODULE_NAME "freerdp-locale")
set(MODULE_PREFIX "FREERDP_LOCALE")

set(SRCS keyboard_layout.c keyboard.c locale.c liblocale.h)

set(X11_SRCS keyboard_x11.c keyboard_x11.h xkb_layout_ids.c xkb_layout_ids.h)

set(XKBFILE_SRCS keyboard_xkbfile.c keyboard_xkbfile.h)

set(SUN_SRCS keyboard_sun.c keyboard_sun.h)

set(APPLE_SRCS keyboard_apple.c keyboard_apple.h)

include(CMakeDependentOption)
cmake_dependent_option(WITH_KEYBOARD_LAYOUT_FROM_FILE "Use keyboard definitions from JSON file" OFF WITH_WINPR_JSON OFF)
if(WITH_KEYBOARD_LAYOUT_FROM_FILE)
  freerdp_definition_add(FREERDP_RESOURCE_ROOT="${FREERDP_RESOURCE_ROOT}")
  freerdp_definition_add(WITH_KEYBOARD_LAYOUT_FROM_FILE)

  install(FILES KeyboardLayoutMap.json DESTINATION ${FREERDP_RESOURCE_ROOT})
endif()

if(CMAKE_SYSTEM_NAME MATCHES Solaris)
  set(WITH_SUN true)
endif()

if(APPLE AND (NOT IOS))
  list(APPEND SRCS ${APPLE_SRCS})
  find_library(CARBON Carbon)
  freerdp_library_add(${CARBON})
endif()

if(APPLE)
  find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
  freerdp_library_add(${CORE_FOUNDATION})
endif()

if(WITH_X11)
  find_package(X11 REQUIRED)

  freerdp_definition_add(WITH_X11)
  freerdp_include_directory_add(${X11_INCLUDE_DIR})
  list(APPEND SRCS ${X11_SRCS})
  freerdp_library_add(${X11_LIBRARIES})

  if(WITH_SUN)
    freerdp_definition_add(WITH_SUN)
    list(APPEND SRCS ${SUN_SRCS})
  endif()

  if(X11_Xkbfile_FOUND AND (NOT APPLE))
    freerdp_definition_add(WITH_XKBFILE)
    freerdp_include_directory_add(${X11_Xkbfile_INCLUDE_PATH})
    list(APPEND SRCS ${XKBFILE_SRCS})
    freerdp_library_add(${X11_Xkbfile_LIB})
  else()
    list(APPEND SRCS ${X11_KEYMAP_SRCS})
  endif()
endif()

if(WITH_WAYLAND)
  freerdp_definition_add(WITH_WAYLAND)
endif()

freerdp_module_add(${SRCS})

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()
