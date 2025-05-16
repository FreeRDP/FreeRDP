# - Find sso-mib
# Find the sso-mib library

# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright 2025 Siemens

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_SSO_MIB sso-mib>=0.4.0)

if(PC_SSO_MIB_FOUND)
  find_path(SSO_MIB_INCLUDE_DIR NAMES sso-mib/sso-mib.h HINTS ${PC_SSO_MIB_INCLUDEDIR})
  find_library(SSO_MIB_LIBRARY NAMES sso-mib HINTS ${PC_SSO_MIB_LIBRARYDIR})
else()
  set(SSO_MIB_ROOT_DIR ${SSO_MIB_EXTERNAL_DIR})
  message(STATUS "SSO_MIB not found through PkgConfig, trying external ${SSO_MIB_ROOT_DIR}")
  find_path(SSO_MIB_INCLUDE_DIR NAMES sso-mib/sso-mib.h HINTS ${SSO_MIB_ROOT_DIR}/include)
  find_library(SSO_MIB_LIBRARY NAMES sso-mib HINTS ${SSO_MIB_ROOT_DIR}/lib)

  # Dependencies
  pkg_check_modules(GLIB REQUIRED glib-2.0)
  pkg_check_modules(GIO REQUIRED gio-2.0)
  pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
  pkg_check_modules(UUID REQUIRED uuid)
  if(GLIB_FOUND AND GIO_FOUND AND JSON_GLIB_FOUND AND UUID_FOUND)
    set(PC_SSO_MIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${JSON_GLIB_INCLUDE_DIRS} ${UUID_INCLUDE_DIRS})
    set(PC_SSO_MIB_LIBRARIES ${GLIB_LIBRARIES} ${GIO_LIBRARIES} ${JSON_GLIB_LIBRARIES} ${UUID_LIBRARIES})
  endif()
endif()

find_package_handle_standard_args(SSO_MIB DEFAULT_MSG SSO_MIB_LIBRARY SSO_MIB_INCLUDE_DIR)

if(SSO_MIB_FOUND)
  set(SSO_MIB_LIBRARIES ${SSO_MIB_LIBRARY} ${PC_SSO_MIB_LIBRARIES})
  set(SSO_MIB_INCLUDE_DIRS ${SSO_MIB_INCLUDE_DIR} ${PC_SSO_MIB_INCLUDE_DIRS})
endif()

mark_as_advanced(SSO_MIB_INCLUDE_DIR SSO_MIB_LIBRARY)
