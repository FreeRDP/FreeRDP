# - Find Xcursor
# Find the Xcursor libraries
#
#  This module defines the following variables:
#     XCURSOR_FOUND        - true if XCURSOR_INCLUDE_DIR & XCURSOR_LIBRARY are found
#     XCURSOR_LIBRARIES    - Set when XCURSOR_LIBRARY is found
#     XCURSOR_INCLUDE_DIRS - Set when XCURSOR_INCLUDE_DIR is found
#
#     XCURSOR_INCLUDE_DIR  - where to find Xcursor.h, etc.
#     XCURSOR_LIBRARY      - the Xcursor library
#

#=============================================================================
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

find_path(XCURSOR_INCLUDE_DIR NAMES X11/Xcursor/Xcursor.h
          PATH_SUFFIXES X11/Xcursor
          DOC "The Xcursor include directory"
)

find_library(XCURSOR_LIBRARY NAMES Xcursor
          DOC "The Xcursor library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xcursor DEFAULT_MSG XCURSOR_LIBRARY XCURSOR_INCLUDE_DIR)

if(XCURSOR_FOUND)
  set( XCURSOR_LIBRARIES ${XCURSOR_LIBRARY} )
  set( XCURSOR_INCLUDE_DIRS ${XCURSOR_INCLUDE_DIR} )
endif()

mark_as_advanced(XCURSOR_INCLUDE_DIR XCURSOR_LIBRARY)

