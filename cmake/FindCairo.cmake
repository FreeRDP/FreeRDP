# - Try to find the CAIRO library
# Once done this will define
#
#  CAIRO_ROOT_DIR - Set this variable to the root installation of CAIRO
#
# Read-Only variables:
#  CAIRO_FOUND - system has the CAIRO library
#  CAIRO_INCLUDE_DIR - the CAIRO include directory
#  CAIRO_LIBRARIES - The libraries needed to use CAIRO
#  CAIRO_VERSION - This is set to $major.$minor.$revision (eg. 0.9.8)

#=============================================================================
# Copyright 2012 Dmitry Baryshnikov <polimax at mail dot ru>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(_CAIRO cairo)
endif(PKG_CONFIG_FOUND)

set(_CAIRO_ROOT_HINTS $ENV{CAIRO} ${CAIRO_ROOT_DIR})
set(_CAIRO_ROOT_PATHS $ENV{CAIRO}/src /usr /usr/local)
set(_CAIRO_ROOT_HINTS_AND_PATHS HINTS ${_CAIRO_ROOT_HINTS} PATHS ${_CAIRO_ROOT_PATHS})

find_path(CAIRO_INCLUDE_DIR NAMES cairo.h HINTS ${_CAIRO_INCLUDEDIR} ${_CAIRO_ROOT_HINTS_AND_PATHS}
          PATH_SUFFIXES include "include/cairo"
)

if(NOT PKGCONFIG_FOUND AND WIN32 AND NOT CYGWIN)
  # MINGW should go here too
  if(MSVC)
    # Implementation details:
    # We are using the libraries located in the VC subdir instead of the parent directory even though :
    find_library(CAIRO_DEBUG NAMES cairod ${_CAIRO_ROOT_HINTS_AND_PATHS} PATH_SUFFIXES "lib" "VC" "lib/VC")

    find_library(CAIRO_RELEASE NAMES cairo ${_CAIRO_ROOT_HINTS_AND_PATHS} PATH_SUFFIXES "lib" "VC" "lib/VC")

    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
      if(NOT CAIRO_DEBUG)
        set(CAIRO_DEBUG ${CAIRO_RELEASE})
      endif(NOT CAIRO_DEBUG)
      set(CAIRO_LIBRARIES optimized ${CAIRO_RELEASE} debug ${CAIRO_DEBUG})
    else()
      set(CAIRO_LIBRARIES ${CAIRO_RELEASE})
    endif()
    mark_as_advanced(CAIRO_DEBUG CAIRO_RELEASE)
  elseif(MINGW)
    # same player, for MingW
    find_library(CAIRO NAMES cairo ${_CAIRO_ROOT_HINTS_AND_PATHS} PATH_SUFFIXES "lib" "lib/MinGW")

    mark_as_advanced(CAIRO)
    set(CAIRO_LIBRARIES ${CAIRO})
  else(MSVC)
    # Not sure what to pick for -say- intel, let's use the toplevel ones and hope someone report issues:
    find_library(CAIRO NAMES cairo HINTS ${_CAIRO_LIBDIR} ${_CAIRO_ROOT_HINTS_AND_PATHS} PATH_SUFFIXES lib)

    mark_as_advanced(CAIRO)
    set(CAIRO_LIBRARIES ${CAIRO})
  endif(MSVC)
else()

  find_library(
    CAIRO_LIBRARY NAMES cairo HINTS ${_CAIRO_LIBDIR} ${_CAIRO_ROOT_HINTS_AND_PATHS} PATH_SUFFIXES "lib" "local/lib"
  )

  mark_as_advanced(CAIRO_LIBRARY)

  # compat defines
  set(CAIRO_LIBRARIES ${CAIRO_LIBRARY})

endif()

#message( STATUS "Cairo_FIND_VERSION=${Cairo_FIND_VERSION}.")
#message( STATUS "CAIRO_INCLUDE_DIR=${CAIRO_INCLUDE_DIR}.")

# Fetch version from cairo-version.h if a version was requested by find_package()
if(CAIRO_INCLUDE_DIR AND Cairo_FIND_VERSION)
  file(READ "${CAIRO_INCLUDE_DIR}/cairo-version.h" _CAIRO_VERSION_H_CONTENTS)
  string(REGEX REPLACE "^(.*\n)?#define[ \t]+CAIRO_VERSION_MAJOR[ \t]+([0-9]+).*" "\\2" CAIRO_VERSION_MAJOR
                       ${_CAIRO_VERSION_H_CONTENTS}
  )
  string(REGEX REPLACE "^(.*\n)?#define[ \t]+CAIRO_VERSION_MINOR[ \t]+([0-9]+).*" "\\2" CAIRO_VERSION_MINOR
                       ${_CAIRO_VERSION_H_CONTENTS}
  )
  string(REGEX REPLACE "^(.*\n)?#define[ \t]+CAIRO_VERSION_MICRO[ \t]+([0-9]+).*" "\\2" CAIRO_VERSION_MICRO
                       ${_CAIRO_VERSION_H_CONTENTS}
  )
  set(CAIRO_VERSION ${CAIRO_VERSION_MAJOR}.${CAIRO_VERSION_MINOR}.${CAIRO_VERSION_MICRO}
      CACHE INTERNAL "The version number for Cairo libraries"
  )
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Cairo REQUIRED_VARS CAIRO_LIBRARIES CAIRO_INCLUDE_DIR VERSION_VAR CAIRO_VERSION)

mark_as_advanced(CAIRO_INCLUDE_DIR CAIRO_LIBRARIES)
