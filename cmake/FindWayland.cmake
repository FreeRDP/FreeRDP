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
#

#=============================================================================
# Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
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

find_path(WAYLAND_INCLUDE_DIR NAMES wayland-client.h
          DOC "The Wayland include directory"
)

find_library(WAYLAND_LIBRARY NAMES wayland-client
          DOC "The Wayland client library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WAYLAND DEFAULT_MSG WAYLAND_LIBRARY WAYLAND_INCLUDE_DIR)

if(WAYLAND_FOUND)
  set( WAYLAND_LIBRARIES ${WAYLAND_LIBRARY} )
  set( WAYLAND_INCLUDE_DIRS ${WAYLAND_INCLUDE_DIR} )
endif()

mark_as_advanced(WAYLAND_INCLUDE_DIR WAYLAND_LIBRARY)

