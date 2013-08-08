# - Find Pixman
# Find the Pixman libraries
#
#  This module defines the following variables:
#     PIXMAN_FOUND        - true if PIXMAN_INCLUDE_DIR & PIXMAN_LIBRARY are found
#     PIXMAN_LIBRARIES    - Set when PIXMAN_LIBRARY is found
#     PIXMAN_INCLUDE_DIRS - Set when PIXMAN_INCLUDE_DIR is found
#
#     PIXMAN_INCLUDE_DIR  - where to find pixman.h, etc.
#     PIXMAN_LIBRARY      - the Pixman library
#

#=============================================================================
# Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

find_path(PIXMAN_INCLUDE_DIR NAMES pixman.h PATH_SUFFIXES pixman-1)

find_library(PIXMAN_LIBRARY NAMES pixman-1)

find_package_handle_standard_args(pixman-1 DEFAULT_MSG PIXMAN_LIBRARY PIXMAN_INCLUDE_DIR)

if(PIXMAN-1_FOUND)
	set(PIXMAN_LIBRARIES ${PIXMAN_LIBRARY})
	set(PIXMAN_INCLUDE_DIRS ${PIXMAN_INCLUDE_DIR})
endif()

mark_as_advanced(PIXMAN_INCLUDE_DIR PIXMAN_LIBRARY)
