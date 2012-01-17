# - Find X11
# Find the X11 libraries
#
#  This module defines the following variables:
#     X11_FOUND        - true if X11_INCLUDE_DIR & X11_LIBRARY are found
#     X11_LIBRARIES    - Set when X11_LIBRARY is found
#     X11_INCLUDE_DIRS - Set when X11_INCLUDE_DIR is found
#
#     X11_INCLUDE_DIR  - where to find Xlib.h, etc.
#     X11_LIBRARY      - the X11 library
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

find_path(X11_INCLUDE_DIR NAMES X11/Xlib.h
          PATH_SUFFIXES X11
          DOC "The X11 include directory"
)

find_library(X11_LIBRARY NAMES X11
          DOC "The X11 library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(X11 DEFAULT_MSG X11_LIBRARY X11_INCLUDE_DIR)

if(X11_FOUND)
  set( X11_LIBRARIES ${X11_LIBRARY} )
  set( X11_INCLUDE_DIRS ${X11_INCLUDE_DIR} )
endif()

mark_as_advanced(X11_INCLUDE_DIR X11_LIBRARY)

