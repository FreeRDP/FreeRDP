# - Find Cmockery
# Find the Cmockery libraries
#
#  This module defines the following variables:
#     CMOCKERY_FOUND        - true if CMOCKERY_INCLUDE_DIR & CMOCKERY_LIBRARY are found
#     CMOCKERY_LIBRARIES    - Set when CMOCKERY_LIBRARY is found
#     CMOCKERY_INCLUDE_DIRS - Set when CMOCKERY_INCLUDE_DIR is found
#

#=============================================================================
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

find_path(CMOCKERY_INCLUDE_DIR NAMES cmockery.h
          PATH_SUFFIXES google)

find_library(CMOCKERY_LIBRARY NAMES cmockery
          DOC "The Cmockery library")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Cmockery DEFAULT_MSG CMOCKERY_LIBRARY CMOCKERY_INCLUDE_DIR)

if(CMOCKERY_FOUND)
	set(CMOCKERY_LIBRARIES ${CMOCKERY_LIBRARY})
	set(CMOCKERY_INCLUDE_DIRS ${CMOCKERY_INCLUDE_DIR})
endif()

mark_as_advanced(CMOCKERY_INCLUDE_DIR CMOCKERY_LIBRARY)

