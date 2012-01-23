# - Find DirectFB
# Find the DirectFB libraries
#
#  This module defines the following variables:
#     DIRECTFB_FOUND        - true if DIRECTFB_INCLUDE_DIR & DIRECTFB_LIBRARY are found
#     DIRECTFB_LIBRARIES    - Set when DIRECTFB_LIBRARY is found
#     DIRECTFB_INCLUDE_DIRS - Set when DIRECTFB_INCLUDE_DIR is found
#
#     DIRECTFB_INCLUDE_DIR  - where to find CUnit.h, etc.
#     DIRECTFB_LIBRARY      - the cunit library
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

find_path(DIRECTFB_INCLUDE_DIR NAMES directfb.h
          PATH_SUFFIXES directfb
          DOC "The directfb include directory"
)

find_library(DIRECTFB_LIBRARY NAMES directfb
          DOC "The DirectFB library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DirectFB DEFAULT_MSG DIRECTFB_LIBRARY DIRECTFB_INCLUDE_DIR)

if(DIRECTFB_FOUND)
  set( DIRECTFB_LIBRARIES ${DIRECTFB_LIBRARY} )
  set( DIRECTFB_INCLUDE_DIRS ${DIRECTFB_INCLUDE_DIR} )
endif()

mark_as_advanced(DIRECTFB_INCLUDE_DIR DIRECTFB_LIBRARY)

