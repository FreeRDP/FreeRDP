############################################################################
# FindV4L.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# Find the v4l library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  v4l1 - If the v4l1 library has been found
#  v4l2 - If the v4l2 library has been found
#  v4lconvert - If the v4lconvert library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  V4L_FOUND - The v4l library has been found
#  V4L_TARGETS - The list of the names of the CMake targets for the v4l libraries
#  V4L_TARGET - The name of the CMake target for the v4l library
#  V4L_convert_TARGET - The name of the CMake target for the v4lconvert library
#  HAVE_LINUX_VIDEODEV_H - If the v4l library provides the linux/videodev.h header file
#  HAVE_LINUX_VIDEODEV2_H - If the v4l library provides the linux/videodev2.h header file

set(_V4L_REQUIRED_VARS V4L_TARGETS V4L_TARGET V4L_convert_TARGET)
set(_V4L_CACHE_VARS ${_V4L_REQUIRED_VARS} HAVE_LINUX_VIDEODEV_H HAVE_LINUX_VIDEODEV2_H)

set(_V4L_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

find_path(_V4L1_INCLUDE_DIRS NAMES linux/videodev.h HINTS _V4L_ROOT_PATHS PATH_SUFFIXES include)
if(_V4L1_INCLUDE_DIRS)
  set(HAVE_LINUX_VIDEODEV_H 1)
endif()
find_path(_V4L2_INCLUDE_DIRS NAMES linux/videodev2.h HINTS _V4L_ROOT_PATHS PATH_SUFFIXES include)
if(_V4L2_INCLUDE_DIRS)
  set(HAVE_LINUX_VIDEODEV2_H 1)
endif()

if(_V4L1_INCLUDE_DIRS OR _V4L2_INCLUDE_DIRS)
  find_library(_V4L1_LIBRARY NAMES v4l1 HINTS _V4L_ROOT_PATHS PATH_SUFFIXES bin lib)
  find_library(_V4L2_LIBRARY NAMES v4l2 HINTS _V4L_ROOT_PATHS PATH_SUFFIXES bin lib)
  find_library(_V4LCONVERT_LIBRARY NAMES v4lconvert HINTS _V4L_ROOT_PATHS PATH_SUFFIXES bin lib)
endif()

if(_V4L1_LIBRARY OR _V4L2_LIBRARY OR _V4LCONVERT_LIBRARY)
  set(V4L_TARGETS)
  if(_V4L2_LIBRARY)
    add_library(v4l2 UNKNOWN IMPORTED)
    set_target_properties(
      v4l2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_V4L2_INCLUDE_DIRS}" IMPORTED_LOCATION "${_V4L2_LIBRARY}"
    )
    set(V4L_TARGET v4l2)
    list(APPEND V4L_TARGETS v4l2)
    set(_V4L_INCLUDE_DIRS "${_V4L2_INCLUDE_DIRS}")
  elseif(_V4L1_LIBRARY)
    add_library(v4l1 UNKNOWN IMPORTED)
    set_target_properties(
      v4l1 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_V4L1_INCLUDE_DIRS}" IMPORTED_LOCATION "${_V4L1_LIBRARY}"
    )
    set(V4L_TARGET v4l1)
    list(APPEND V4L_TARGETS v4l1)
    set(_V4L_INCLUDE_DIRS "${_V4L1_INCLUDE_DIRS}")
  endif()
  if(_V4LCONVERT_LIBRARY)
    add_library(v4lconvert UNKNOWN IMPORTED)
    set_target_properties(
      v4lconvert PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_V4L_INCLUDE_DIRS}" IMPORTED_LOCATION
                                                                                 "${_V4LCONVERT_LIBRARY}"
    )
    set(V4L_convert_TARGET v4lconvert)
    list(APPEND V4L_TARGETS v4lconvert)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(V4L REQUIRED_VARS ${_V4L_REQUIRED_VARS})
mark_as_advanced(${_V4L_CACHE_VARS})
