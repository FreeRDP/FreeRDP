# - Find XTEST
# Find the XTEST libraries
#
#  This module defines the following variables:
#     XTEST_FOUND        - true if XTEST_INCLUDE_DIR & XTEST_LIBRARY are found
#     XTEST_LIBRARIES    - Set when XTEST_LIBRARY is found
#     XTEST_INCLUDE_DIRS - Set when XTEST_INCLUDE_DIR is found
#
#     XTEST_INCLUDE_DIR  - where to find XTest.h, etc.
#     XTEST_LIBRARY      - the XTEST library
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

find_path(XTEST_INCLUDE_DIR NAMES X11/extensions/XTest.h
          PATH_SUFFIXES X11/extensions
          DOC "The XTest include directory"
)

find_library(XTEST_LIBRARY NAMES Xtst
          DOC "The XTest library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XTest DEFAULT_MSG XTEST_LIBRARY XTEST_INCLUDE_DIR)

if(XTEST_FOUND)
  set( XTEST_LIBRARIES ${XTEST_LIBRARY} )
  set( XTEST_INCLUDE_DIRS ${XTEST_INCLUDE_DIR} )
endif()

mark_as_advanced(XTEST_INCLUDE_DIR XTEST_LIBRARY)

