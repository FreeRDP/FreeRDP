# - Find XRender
# Find the XRender libraries
#
# This module defines the following variables:
# 	XRENDER_FOUND - true if XRENDER_INCLUDE_DIR & XRENDER_LIBRARY are found
# 	XRENDER_LIBRARIES - Set when Xrender_LIBRARY is found
# 	XRENDER_INCLUDE_DIRS - Set when Xrender_INCLUDE_DIR is found
#
# 	XRENDER_INCLUDE_DIR - where to find Xrender.h, etc.
# 	XRENDER_LIBRARY - the Xrender library
#

#=============================================================================
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

find_path(XRENDER_INCLUDE_DIR NAMES X11/extensions/Xrender.h
          PATHS /opt/X11/include
          DOC "The Xrender include directory")

find_library(XRENDER_LIBRARY NAMES Xrender
          PATHS /opt/X11/lib
          DOC "The Xrender library")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xrender DEFAULT_MSG XRENDER_LIBRARY XRENDER_INCLUDE_DIR)

if(XRENDER_FOUND)

	set(XRENDER_LIBRARIES ${XRENDER_LIBRARY})
	set(XRENDER_INCLUDE_DIRS ${XRENDER_INCLUDE_DIR})

endif()

mark_as_advanced(XRENDER_INCLUDE_DIR XRENDER_LIBRARY)
