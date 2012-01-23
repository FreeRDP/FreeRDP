# - Find XDAMAGE
# Find the XDAMAGE libraries
#
#  This module defines the following variables:
#     XDAMAGE_FOUND        - true if XDAMAGE_INCLUDE_DIR & XDAMAGE_LIBRARY are found
#     XDAMAGE_LIBRARIES    - Set when XDAMAGE_LIBRARY is found
#     XDAMAGE_INCLUDE_DIRS - Set when XDAMAGE_INCLUDE_DIR is found
#
#     XDAMAGE_INCLUDE_DIR  - where to find Xdamage.h, etc.
#     XDAMAGE_LIBRARY      - the XDAMAGE library
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

find_path(XDAMAGE_INCLUDE_DIR NAMES X11/extensions/Xdamage.h
          PATH_SUFFIXES X11/extensions
          DOC "The Xdamage include directory"
)

find_library(XDAMAGE_LIBRARY NAMES Xdamage
          DOC "The Xdamage library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xdamage DEFAULT_MSG XDAMAGE_LIBRARY XDAMAGE_INCLUDE_DIR)

if(XDAMAGE_FOUND)
  set( XDAMAGE_LIBRARIES ${XDAMAGE_LIBRARY} )
  set( XDAMAGE_INCLUDE_DIRS ${XDAMAGE_INCLUDE_DIR} )
endif()


mark_as_advanced(XDAMAGE_INCLUDE_DIR XDAMAGE_LIBRARY)
