# - Find CUnit
# Find the CUnit libraries
#
#  This module defines the following variables:
#     CUNIT_FOUND        - true if CUNIT_INCLUDE_DIR & CUNIT_LIBRARY are found
#     CUNIT_LIBRARIES    - Set when CUNIT_LIBRARY is found
#     CUNIT_INCLUDE_DIRS - Set when CUNIT_INCLUDE_DIR is found
#
#     CUNIT_INCLUDE_DIR  - where to find CUnit.h, etc.
#     CUNIT_LIBRARY      - the cunit library
#

#=============================================================================
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
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

find_path(CUNIT_INCLUDE_DIR NAMES CUnit.h
          PATH_SUFFIXES CUnit
          DOC "The CUnit include directory"
)

find_library(CUNIT_LIBRARY NAMES cunit
          DOC "The CUnit library"
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CUnit DEFAULT_MSG CUNIT_LIBRARY CUNIT_INCLUDE_DIR)

if(CUNIT_FOUND)
  set( CUNIT_LIBRARIES ${CUNIT_LIBRARY} )
  set( CUNIT_INCLUDE_DIRS ${CUNIT_INCLUDE_DIR} )
endif()

mark_as_advanced(CUNIT_INCLUDE_DIR CUNIT_LIBRARY)
