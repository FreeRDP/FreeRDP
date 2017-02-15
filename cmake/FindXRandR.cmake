# - Find XRANDR
# Find the XRANDR libraries
#
#  This module defines the following variables:
#     XRANDR_FOUND        - true if XRANDR_INCLUDE_DIR & XRANDR_LIBRARY are found
#     XRANDR_LIBRARIES    - Set when XRANDR_LIBRARY is found
#     XRANDR_INCLUDE_DIRS - Set when XRANDR_INCLUDE_DIR is found
#
#     XRANDR_INCLUDE_DIR  - where to find Xrandr.h, etc.
#     XRANDR_LIBRARY      - the XRANDR library
#

#=============================================================================
# Copyright 2012 Alam Arias <Alam.GBC@gmail.com>
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

find_path(XRANDR_INCLUDE_DIR NAMES X11/extensions/Xrandr.h
          PATH_SUFFIXES X11/extensions
          PATHS /opt/X11/include
          DOC "The XRANDR include directory"
)

find_library(XRANDR_LIBRARY NAMES Xrandr
          PATHS /opt/X11/lib
          DOC "The XRANDR library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XRANDR DEFAULT_MSG XRANDR_LIBRARY XRANDR_INCLUDE_DIR)

if(XRANDR_FOUND)
  set( XRANDR_LIBRARIES ${XRANDR_LIBRARY} )
  set( XRANDR_INCLUDE_DIRS ${XRANDR_INCLUDE_DIR} )
endif()

mark_as_advanced(XRANDR_INCLUDE_DIR XRANDR_LIBRARY)

