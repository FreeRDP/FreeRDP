# Try to find the soxr library
#
# Copyright 2018 Thincast Technologies GmbH
# Copyright 2018 Armin Novak <armin.novak@thincast.com>
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
#
# Once done this will define
#
#  SOXR_ROOT - A list of search hints
#
#  SOXR_FOUND - system has soxr
#  SOXR_INCLUDE_DIR - the soxr include directory
#  SOXR_LIBRARIES - libsoxr library

if (UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_SOXR QUIET soxr)
endif (UNIX AND NOT ANDROID)

if (SOXR_INCLUDE_DIR AND SOXR_LIBRARY)
	set(SOXR_FIND_QUIETLY TRUE)
endif (SOXR_INCLUDE_DIR AND SOXR_LIBRARY)

find_path(SOXR_INCLUDE_DIR NAMES soxr.h 
	PATH_SUFFIXES include
	HINTS ${SOXR_ROOT} ${PC_SOXR_INCLUDE_DIRS})
find_library(SOXR_LIBRARY
	 NAMES soxr
	 PATH_SUFFIXES lib
	 HINTS ${SOXR_ROOT} ${PC_SOXR_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(soxr DEFAULT_MSG SOXR_LIBRARY SOXR_INCLUDE_DIR)

if (SOXR_INCLUDE_DIR AND SOXR_LIBRARY)
	set(SOXR_FOUND TRUE)
	set(SOXR_INCLUDE_DIRS ${SOXR_INCLUDE_DIR})
	set(SOXR_LIBRARIES ${SOXR_LIBRARY})
endif (SOXR_INCLUDE_DIR AND SOXR_LIBRARY)

if (SOXR_FOUND)
	if (NOT SOXR_FIND_QUIETLY)
		message(STATUS "Found soxr: ${SOXR_LIBRARIES}")
	endif (NOT SOXR_FIND_QUIETLY)
else (SOXR_FOUND)
	if (SOXR_FIND_REQUIRED)
		message(FATAL_ERROR "soxr was not found")
	endif(SOXR_FIND_REQUIRED)
endif (SOXR_FOUND)

mark_as_advanced(SOXR_INCLUDE_DIR SOXR_LIBRARY)

