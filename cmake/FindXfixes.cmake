# - Find XFIXES
# Find the XFIXES libraries
#
#  This module defines the following variables:
#     XFIXES_FOUND        - true if XFIXES_INCLUDE_DIR & XFIXES_LIBRARY are found
#     XFIXES_LIBRARIES    - Set when XFIXES_LIBRARY is found
#     XFIXES_INCLUDE_DIRS - Set when XFIXES_INCLUDE_DIR is found
#
#     XFIXES_INCLUDE_DIR  - where to find Xfixes.h, etc.
#     XFIXES_LIBRARY      - the XFIXES library
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

find_path(XFIXES_INCLUDE_DIR NAMES X11/extensions/Xfixes.h
          PATH_SUFFIXES X11/extensions
          DOC "The Xfixes include directory"
)

find_library(XFIXES_LIBRARY NAMES Xfixes
          DOC "The Xfixes library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xfixes DEFAULT_MSG XFIXES_LIBRARY XFIXES_INCLUDE_DIR)

if(XFIXES_FOUND)
  set( XFIXES_LIBRARIES ${XFIXES_LIBRARY} )
  set( XFIXES_INCLUDE_DIRS ${XFIXES_INCLUDE_DIR} )
endif()

mark_as_advanced(XFIXES_INCLUDE_DIR XFIXES_LIBRARY)

