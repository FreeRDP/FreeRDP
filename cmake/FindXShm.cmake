# - Find XSHM
# Find the XSHM libraries
#
#  This module defines the following variables:
#     XSHM_FOUND        - true if XSHM_INCLUDE_DIR & XSHM_LIBRARY are found
#     XSHM_LIBRARIES    - Set when XSHM_LIBRARY is found
#     XSHM_INCLUDE_DIRS - Set when XSHM_INCLUDE_DIR is found
#
#     XSHM_INCLUDE_DIR  - where to find XShm.h, etc.
#     XSHM_LIBRARY      - the XSHM library
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

find_path(XSHM_INCLUDE_DIR NAMES X11/extensions/XShm.h
          PATH_SUFFIXES X11/extensions
          DOC "The XShm include directory"
)

find_library(XSHM_LIBRARY NAMES Xext
          DOC "The XShm library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XShm DEFAULT_MSG XSHM_LIBRARY XSHM_INCLUDE_DIR)

if(XSHM_FOUND)
  set( XSHM_LIBRARIES ${XSHM_LIBRARY} )
  set( XSHM_INCLUDE_DIRS ${XSHM_INCLUDE_DIR} )
endif()

mark_as_advanced(XSHM_INCLUDE_DIR XSHM_LIBRARY)

