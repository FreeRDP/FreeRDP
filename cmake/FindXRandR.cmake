# - Find XRandR
# Find the XRandR libraries
#
#  This module defines the following variables:
#     XRandR_FOUND        - true if XRANDR_INCLUDE_DIR & XRANDR_LIBRARY are found
#     XRandR_LIBRARIES    - Set when XRANDR_LIBRARY is found
#     XRandR_INCLUDE_DIRS - Set when XRANDR_INCLUDE_DIR is found
#
#     XRandR_INCLUDE_DIR  - where to find Xrandr.h, etc.
#     XRandR_LIBRARY      - the XRANDR library
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

find_path(XRandR_INCLUDE_DIR NAMES X11/extensions/Xrandr.h
          PATH_SUFFIXES X11/extensions
          PATHS /opt/X11/include
		  DOC "The XRandR include directory"
)

find_library(XRandR_LIBRARY NAMES Xrandr
          PATHS /opt/X11/lib
		  DOC "The XRandR library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XRandR DEFAULT_MSG XRandR_LIBRARY XRandR_INCLUDE_DIR)

if(XRandR_FOUND)
  set( XRandR_LIBRARIES ${XRandR_LIBRARY} )
  set( XRandR_INCLUDE_DIRS ${XRandR_INCLUDE_DIR} )
endif()

mark_as_advanced(XRandR_INCLUDE_DIR XRandR_LIBRARY)

