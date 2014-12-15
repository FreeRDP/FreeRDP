# - Find Xi
# Find the Xi libraries
#
#  This module defines the following variables:
#     XI_FOUND        - true if XI_INCLUDE_DIR & XI_LIBRARY are found
#     XI_LIBRARIES    - Set when XI_LIBRARY is found
#     XI_INCLUDE_DIRS - Set when XI_INCLUDE_DIR is found
#
#     XI_INCLUDE_DIR  - where to find XInput2.h, etc.
#     XI_LIBRARY      - the Xi library
#

#=============================================================================
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

include(CheckSymbolExists)

find_path(XI_INCLUDE_DIR NAMES X11/extensions/XInput2.h
          PATHS /opt/X11/include
          DOC "The Xi include directory")

find_library(XI_LIBRARY NAMES Xi
          PATHS /opt/X11/lib
          DOC "The Xi library")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xi DEFAULT_MSG XI_LIBRARY XI_INCLUDE_DIR)

if(XI_FOUND)
	check_symbol_exists(XITouchClass "${XI_INCLUDE_DIR}/X11/extensions/XInput2.h" HAVE_XI_TOUCH_CLASS)

	if (NOT HAVE_XI_TOUCH_CLASS)
		set(XI_FOUND 0)
	endif()
endif()

if(XI_FOUND)
	set(XI_LIBRARIES ${XI_LIBRARY})
	set(XI_INCLUDE_DIRS ${XI_INCLUDE_DIR})
endif()

mark_as_advanced(XI_INCLUDE_DIR XI_LIBRARY)

