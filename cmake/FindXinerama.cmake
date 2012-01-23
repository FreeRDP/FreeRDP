# - Find XINERAMA
# Find the XINERAMA libraries
#
#  This module defines the following variables:
#     XINERAMA_FOUND        - true if XINERAMA_INCLUDE_DIR & XINERAMA_LIBRARY are found
#     XINERAMA_LIBRARIES    - Set when XINERAMA_LIBRARY is found
#     XINERAMA_INCLUDE_DIRS - Set when XINERAMA_INCLUDE_DIR is found
#
#     XINERAMA_INCLUDE_DIR  - where to find Xinerama.h, etc.
#     XINERAMA_LIBRARY      - the XINERAMA library
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

find_path(XINERAMA_INCLUDE_DIR NAMES X11/extensions/Xinerama.h
          PATH_SUFFIXES X11/extensions
          DOC "The Xinerama include directory"
)

find_library(XINERAMA_LIBRARY NAMES Xinerama
          DOC "The Xinerama library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xinerama DEFAULT_MSG XINERAMA_LIBRARY XINERAMA_INCLUDE_DIR)

if(XINERAMA_FOUND)
  set( XINERAMA_LIBRARIES ${XINERAMA_LIBRARY} )
  set( XINERAMA_INCLUDE_DIRS ${XINERAMA_INCLUDE_DIR} )
endif()

mark_as_advanced(XINERAMA_INCLUDE_DIR XINERAMA_LIBRARY)

