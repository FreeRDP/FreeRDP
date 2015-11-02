# - Try to find Gstreamer and its plugins
# Once done, this will define
#
#  GSTREAMER_1_0_FOUND - system has Gstreamer
#  GSTREAMER_1_0_INCLUDE_DIRS - the Gstreamer include directories
#  GSTREAMER_1_0_LIBRARIES - link these to use Gstreamer
#
# Additionally, gstreamer-base is always looked for and required, and
# the following related variables are defined:
#
#  GSTREAMER_1_0_BASE_INCLUDE_DIRS - gstreamer-base's include directory
#  GSTREAMER_1_0_BASE_LIBRARIES - link to these to use gstreamer-base
#
# Optionally, the COMPONENTS keyword can be passed to find_package()
# and Gstreamer plugins can be looked for.  Currently, the following
# plugins can be searched, and they define the following variables if
# found:
#
#  gstreamer-app:        GSTREAMER_1_0_APP_INCLUDE_DIRS and GSTREAMER_1_0_APP_LIBRARIES
#  gstreamer-audio:      GSTREAMER_1_0_AUDIO_INCLUDE_DIRS and GSTREAMER_1_0_AUDIO_LIBRARIES
#  gstreamer-fft:        GSTREAMER_1_0_FFT_INCLUDE_DIRS and GSTREAMER_1_0_FFT_LIBRARIES
#  gstreamer-pbutils:    GSTREAMER_1_0_PBUTILS_INCLUDE_DIRS and GSTREAMER_1_0_PBUTILS_LIBRARIES
#  gstreamer-video:      GSTREAMER_1_0_VIDEO_INCLUDE_DIRS and GSTREAMER_1_0_VIDEO_LIBRARIES
#
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)

# The minimum Gstreamer version we support.
set(GSTREAMER_1_0_MINIMUM_VERSION 1.0.5)

# Helper macro to find a Gstreamer plugin (or Gstreamer itself)
#   _component_prefix is prepended to the _INCLUDE_DIRS and _LIBRARIES variables (eg. "GSTREAMER_1_0_AUDIO")
#   _pkgconfig_name is the component's pkg-config name (eg. "gstreamer-1.0", or "gstreamer-video-1.0").
#   _library is the component's library name (eg. "gstreamer-1.0" or "gstvideo-1.0")
macro(FIND_GSTREAMER_COMPONENT _component_prefix _pkgconfig_name _library)
    # FIXME: The QUIET keyword can be used once we require CMake 2.8.2.

    string(REGEX MATCH "(.*)>=(.*)" _dummy "${_pkgconfig_name}")
    if ("${CMAKE_MATCH_2}" STREQUAL "")
        pkg_check_modules(PC_${_component_prefix} "${_pkgconfig_name} >= ${GStreamer_FIND_VERSION}")
    else ()
        pkg_check_modules(PC_${_component_prefix} ${_pkgconfig_name})
    endif ()
    set(${_component_prefix}_INCLUDE_DIRS ${PC_${_component_prefix}_INCLUDE_DIRS})

    find_library(${_component_prefix}_LIBRARIES
        NAMES ${_library} gstreamer_android
        HINTS ${PC_${_component_prefix}_LIBRARY_DIRS} ${PC_${_component_prefix}_LIBDIR} ${GSTREAMER_1_0_ROOT_DIR}
    )
endmacro()

# ------------------------
# 1. Find Gstreamer itself
# ------------------------

# 1.1. Find headers and libraries
set(GLIB_ROOT_DIR ${GSTREAMER_1_0_ROOT_DIR})
find_package(Glib REQUIRED)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0 gstreamer-1.0 gstreamer-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_BASE gstreamer-base-1.0 gstbase-1.0)

# 1.2. Check Gstreamer version
if (GSTREAMER_1_0_INCLUDE_DIRS)
    if (EXISTS "${GSTREAMER_1_0_INCLUDE_DIRS}/gst/gstversion.h")
        file(READ "${GSTREAMER_1_0_INCLUDE_DIRS}/gst/gstversion.h" GSTREAMER_VERSION_CONTENTS)

        string(REGEX MATCH "#define +GST_VERSION_MAJOR +\\(([0-9]+)\\)" _dummy "${GSTREAMER_VERSION_CONTENTS}")
        set(GSTREAMER_1_0_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +GST_VERSION_MINOR +\\(([0-9]+)\\)" _dummy "${GSTREAMER_1_0_VERSION_CONTENTS}")
        set(GSTREAMER_1_0_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +GST_VERSION_MICRO +\\(([0-9]+)\\)" _dummy "${GSTREAMER_1_0_VERSION_CONTENTS}")
        set(GSTREAMER_1_0_VERSION_MICRO "${CMAKE_MATCH_1}")

        set(GSTREAMER_1_0_VERSION "${GSTREAMER_1_0_VERSION_MAJOR}.${GSTREAMER_1_0_VERSION_MINOR}.${GSTREAMER_1_0_VERSION_MICRO}")
    endif ()
endif ()

# FIXME: With CMake 2.8.3 we can just pass GSTREAMER_1_0_VERSION to FIND_PACKAGE_HANDLE_STANDARD_ARGS as VERSION_VAR
#        and remove the version check here (GSTREAMER_1_0_MINIMUM_VERSION would be passed to FIND_PACKAGE).
set(VERSION_OK TRUE)
if ("${GSTREAMER_1_0_VERSION}" VERSION_LESS "${GSTREAMER_1_0_MINIMUM_VERSION}")
    set(VERSION_OK FALSE)
endif ()

# -------------------------
# 2. Find Gstreamer plugins
# -------------------------

FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_APP gstreamer-app-1.0 gstapp-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_AUDIO gstreamer-audio-1.0 gstaudio-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_FFT gstreamer-fft-1.0 gstfft-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_PBUTILS gstreamer-pbutils-1.0 gstpbutils-1.0)
FIND_GSTREAMER_COMPONENT(GSTREAMER_1_0_VIDEO gstreamer-video-1.0 gstvideo-1.0)

# ------------------------------------------------
# 3. Process the COMPONENTS passed to FIND_PACKAGE
# ------------------------------------------------
set(_GSTREAMER_1_0_REQUIRED_VARS
	Glib_INCLUDE_DIRS
	Glib_LIBRARIES
	GSTREAMER_1_0_INCLUDE_DIRS
	GSTREAMER_1_0_LIBRARIES
	VERSION_OK
	GSTREAMER_1_0_BASE_INCLUDE_DIRS
	GSTREAMER_1_0_BASE_LIBRARIES)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GSTREAMER_1_0 DEFAULT_MSG GSTREAMER_1_0_LIBRARIES GSTREAMER_1_0_INCLUDE_DIRS)

list(APPEND GSTREAMER_1_0_INCLUDE_DIRS ${Glib_INCLUDE_DIRS})
list(APPEND GSTREAMER_1_0_LIBRARIES ${Glib_LIBRARIES})
list(APPEND GSTREAMER_1_0_INCLUDE_DIRS ${GSTREAMER_1_0_BASE_INCLUDE_DIRS})
list(APPEND GSTREAMER_1_0_LIBRARIES ${GSTREAMER_1_0_BASE_LIBRARIES})
list(APPEND GSTREAMER_1_0_INCLUDE_DIRS ${GSTREAMER_1_0_APP_INCLUDE_DIRS})
list(APPEND GSTREAMER_1_0_LIBRARIES ${GSTREAMER_1_0_APP_LIBRARIES})
list(APPEND GSTREAMER_1_0_INCLUDE_DIRS ${GSTREAMER_1_0_VIDEO_INCLUDE_DIRS})
list(APPEND GSTREAMER_1_0_LIBRARIES ${GSTREAMER_1_0_VIDEO_LIBRARIES})

foreach (_component ${Gstreamer_FIND_COMPONENTS})
    set(_gst_component "GSTREAMER_1_0_${_component}")
    string(TOUPPER ${_gst_component} _UPPER_NAME)

    FIND_PACKAGE_HANDLE_STANDARD_ARGS(${_UPPER_NAME} DEFAULT_MSG ${_UPPER_NAME}_INCLUDE_DIRS ${_UPPER_NAME}_LIBRARIES)
    list(APPEND GSTREAMER_1_0_INCLUDE_DIRS ${${_UPPER_NAME}_INCLUDE_DIRS})
    list(APPEND GSTREAMER_1_0_LIBRARIES ${${_UPPER_NAME}_LIBRARIES})
endforeach ()

MARK_AS_ADVANCED(GSTREAMER_1_0_INCLUDE_DIRS GSTREAMER_1_0_LIBRARIES GSTREAMER_1_0_BASE_LIBRARY GSTREAMER_1_0_APP_LIBRARY)

