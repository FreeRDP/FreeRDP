# - Find XKBFILE
# Find the XKBFILE libraries
#
#  This module defines the following variables:
#     XKBFILE_FOUND        - true if XKBFILE_INCLUDE_DIR & XKBFILE_LIBRARY are found
#     XKBFILE_LIBRARIES    - Set when XKBFILE_LIBRARY is found
#     XKBFILE_INCLUDE_DIRS - Set when XKBFILE_INCLUDE_DIR is found
#
#     XKBFILE_INCLUDE_DIR  - where to find XKBfile.h, etc.
#     XKBFILE_LIBRARY      - the xkbfile library
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

find_path(XKBFILE_INCLUDE_DIR NAMES X11/extensions/XKBfile.h
          PATH_SUFFIXES X11/extensions
          DOC "The XKBFile include directory"
)

find_library(XKBFILE_LIBRARY NAMES xkbfile
          DOC "The XKBFile library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XKBFile DEFAULT_MSG XKBFILE_LIBRARY XKBFILE_INCLUDE_DIR)

if(XKBFILE_FOUND)
  set( XKBFILE_LIBRARIES ${XKBFILE_LIBRARY} )
  set( XKBFILE_INCLUDE_DIRS ${XKBFILE_INCLUDE_DIR} )
endif()

mark_as_advanced(XKBFILE_INCLUDE_DIR XKBFILE_LIBRARY)

