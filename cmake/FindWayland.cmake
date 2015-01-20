# - Find Wayland
# Find the Wayland libraries
#
#  This module defines the following variables:
#     WAYLAND_FOUND        - true if WAYLAND_INCLUDE_DIR & WAYLAND_LIBRARY are found
#     WAYLAND_LIBRARIES    - Set when WAYLAND_LIBRARY is found
#     WAYLAND_INCLUDE_DIRS - Set when WAYLAND_INCLUDE_DIR is found
#
#     WAYLAND_INCLUDE_DIR  - where to find wayland-client.h, etc.
#     WAYLAND_LIBRARY      - the Wayland client library
#     WAYLAND_VERSION      - wayland client version if found and pkg-config was used
#

#=============================================================================
# Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
# Copyright 2015 Bernhard Miklautz <bernhard.miklautz@shacknet.at>
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
#=============================================================================

set(REQUIRED_WAYLAND_CLIENT_VERSION 1.3.0)
include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(WAYLAND wayland-client)
endif()

find_path(WAYLAND_INCLUDE_DIR NAMES wayland-client.h
          PATHS ${WAYLAND_INCLUDE_DIRS}
          DOC "The Wayland include directory"
)

find_library(WAYLAND_LIBRARY NAMES wayland-client
          PATHS ${WAYLAND_LIBRARY_DIRS}
          DOC "The Wayland client library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WAYLAND DEFAULT_MSG WAYLAND_LIBRARY WAYLAND_INCLUDE_DIR)

if(WAYLAND_VERSION)
	if (${WAYLAND_VERSION} VERSION_LESS ${REQUIRED_WAYLAND_CLIENT_VERSION})
		message(WARNING "Installed wayland version ${WAYLAND_VERSION} is too old - minimum required version ${REQUIRED_WAYLAND_CLIENT_VERSION}")
		set(WAYLAND_FOUND FALSE)
	endif()
else()
		message(WARNING "Couldn't detect wayland version - no version check is done")
endif()

if(WAYLAND_FOUND)
  set(WAYLAND_LIBRARIES ${WAYLAND_LIBRARY})
  set(WAYLAND_INCLUDE_DIRS ${WAYLAND_INCLUDE_DIR})
endif()

mark_as_advanced(WAYLAND_INCLUDE_DIR WAYLAND_LIBRARY WAYLAND_VERSION)
