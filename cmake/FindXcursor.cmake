# - Find Xcursor
# Find the Xcursor libraries
#
#  This module defines the following variables:
#     Xcursor_FOUND        - True if Xcursor_INCLUDE_DIR & Xcursor_LIBRARY are found
#     Xcursor_LIBRARIES    - Set when Xcursor_LIBRARY is found
#     Xcursor_INCLUDE_DIRS - Set when Xcursor_INCLUDE_DIR is found
#
#     Xcursor_INCLUDE_DIR  - where to find Xcursor.h, etc.
#     Xcursor_LIBRARY      - the Xcursor library
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

find_path(Xcursor_INCLUDE_DIR NAMES Xcursor.h
          PATH_SUFFIXES X11/Xcursor
          DOC "The Xcursor include directory"
)

find_library(Xcursor_LIBRARY NAMES Xcursor
          DOC "The Xcursor library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xcursor DEFAULT_MSG Xcursor_LIBRARY Xcursor_INCLUDE_DIR)

if(Xcursor_FOUND)
  set( Xcursor_LIBRARIES ${Xcursor_LIBRARY} )
  set( Xcursor_INCLUDE_DIRS ${Xcursor_INCLUDE_DIR} )
endif()

mark_as_advanced(Xcursor_INCLUDE_DIR Xcursor_LIBRARY)

