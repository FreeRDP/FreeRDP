# - Find Xext
# Find the Xext libraries
#
#  This module defines the following variables:
#     Xext_FOUND        - true if Xext_INCLUDE_DIR & Xext_LIBRARY are found
#     Xext_LIBRARIES    - Set when Xext_LIBRARY is found
#     Xext_INCLUDE_DIRS - Set when Xext_INCLUDE_DIR is found
#
#     Xext_INCLUDE_DIR  - where to find Xext.h, etc.
#     Xext_LIBRARY      - the Xext library
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

find_path(XEXT_INCLUDE_DIR NAMES X11/extensions/Xext.h
          PATH_SUFFIXES X11/extensions
          DOC "The Xext include directory"
)

find_library(XEXT_LIBRARY NAMES Xext
          DOC "The Xext library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xext DEFAULT_MSG XEXT_LIBRARY XEXT_INCLUDE_DIR)

if(XEXT_FOUND)
  set( XEXT_LIBRARIES ${XEXT_LIBRARY} )
  set( XEXT_INCLUDE_DIRS ${XEXT_INCLUDE_DIR} )
endif()

mark_as_advanced(XEXT_INCLUDE_DIR XEXT_LIBRARY)

