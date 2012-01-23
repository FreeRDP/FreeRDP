# - Find Xv
# Find the Xv libraries
#
#  This module defines the following variables:
#     XV_FOUND        - true if XV_INCLUDE_DIR & XV_LIBRARY are found
#     XV_LIBRARIES    - Set when XV_LIBRARY is found
#     XV_INCLUDE_DIRS - Set when XV_INCLUDE_DIR is found
#
#     XV_INCLUDE_DIR  - where to find Xv.h, etc.
#     XV_LIBRARY      - the Xv library
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

find_path(XV_INCLUDE_DIR NAMES X11/extensions/Xv.h
          PATH_SUFFIXES X11/extensions
          DOC "The Xv include directory"
)

find_library(XV_LIBRARY NAMES Xv
          DOC "The Xv library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xv DEFAULT_MSG XV_LIBRARY XV_INCLUDE_DIR)

if(XV_FOUND)
  set( XV_LIBRARIES ${XV_LIBRARY} )
  set( XV_INCLUDE_DIRS ${XV_INCLUDE_DIR} )
endif()

mark_as_advanced(XV_INCLUDE_DIR XV_LIBRARY)

