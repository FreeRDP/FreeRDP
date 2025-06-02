# - Find sso-mib
# Find the sso-mib library

# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright 2025 Siemens

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_SSO_MIB sso-mib>=0.5.0)

if(PC_SSO_MIB_FOUND)
  find_path(SSO_MIB_INCLUDE_DIR NAMES sso-mib/sso-mib.h HINTS ${PC_SSO_MIB_INCLUDEDIR})
  find_library(SSO_MIB_LIBRARY NAMES sso-mib HINTS ${PC_SSO_MIB_LIBRARYDIR})

  find_package_handle_standard_args(SSO_MIB DEFAULT_MSG SSO_MIB_LIBRARY SSO_MIB_INCLUDE_DIR)

  if(SSO_MIB_FOUND)
    set(SSO_MIB_LIBRARIES ${SSO_MIB_LIBRARY} ${PC_SSO_MIB_LIBRARIES})
    set(SSO_MIB_INCLUDE_DIRS ${SSO_MIB_INCLUDE_DIR} ${PC_SSO_MIB_INCLUDE_DIRS})
  endif()
else()
  include(ExternalProject)
  set(SSO_MIB_EXTERNAL_DIR ${CMAKE_BINARY_DIR}/sso-mib-external)

  set(SSO_MIB_URL https://github.com/siemens/sso-mib.git)
  set(SSO_MIB_VERSION_MAJOR 0)
  set(SSO_MIB_VERSION_MINOR 5)
  set(SSO_MIB_VERSION_PATCH 0)
  set(SSO_MIB_VERSION v${SSO_MIB_VERSION_MAJOR}.${SSO_MIB_VERSION_MINOR}.${SSO_MIB_VERSION_PATCH})
  message(STATUS "Adding sso-mib as ExternalProject from ${SSO_MIB_URL}, version ${SSO_MIB_VERSION}")

  ExternalProject_Add(
    sso-mib-external GIT_REPOSITORY ${SSO_MIB_URL} GIT_TAG ${SSO_MIB_VERSION} PREFIX ${SSO_MIB_EXTERNAL_DIR}
    SOURCE_DIR ${SSO_MIB_EXTERNAL_DIR}/src BINARY_DIR ${SSO_MIB_EXTERNAL_DIR}/build TMP_DIR _deps/tmp
    STAMP_DIR _deps/stamp CONFIGURE_COMMAND meson setup --prefix=${SSO_MIB_EXTERNAL_DIR}/install --libdir=lib/
                                            ${SSO_MIB_EXTERNAL_DIR}/build ${SSO_MIB_EXTERNAL_DIR}/src
    BUILD_COMMAND meson compile -C ${SSO_MIB_EXTERNAL_DIR}/build INSTALL_COMMAND meson install -C
                                                                                 ${SSO_MIB_EXTERNAL_DIR}/build
    UPDATE_COMMAND "" BUILD_BYPRODUCTS ${SSO_MIB_EXTERNAL_DIR}/install/lib/libsso-mib.so
  )

  # Dependencies
  pkg_check_modules(GLIB REQUIRED glib-2.0)
  pkg_check_modules(GIO REQUIRED gio-2.0)
  pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
  pkg_check_modules(UUID REQUIRED uuid)
  if(GLIB_FOUND AND GIO_FOUND AND JSON_GLIB_FOUND AND UUID_FOUND)
    set(PC_SSO_MIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${JSON_GLIB_INCLUDE_DIRS} ${UUID_INCLUDE_DIRS})
    set(PC_SSO_MIB_LIBRARIES ${GLIB_LIBRARIES} ${GIO_LIBRARIES} ${JSON_GLIB_LIBRARIES} ${UUID_LIBRARIES})
  endif()

  set(SSO_MIB_INCLUDE_DIRS ${SSO_MIB_EXTERNAL_DIR}/install/include ${PC_SSO_MIB_INCLUDE_DIRS})
  set(SSO_MIB_LIBRARIES ${SSO_MIB_EXTERNAL_DIR}/install/lib/libsso-mib.so ${PC_SSO_MIB_LIBRARIES})
  if(BUILD_SHARED_LIBS)
    set(SSO_MIB_INSTALL_LIBRARIES
        ${SSO_MIB_EXTERNAL_DIR}/install/lib/libsso-mib.so
        ${SSO_MIB_EXTERNAL_DIR}/install/lib/libsso-mib.so.${SSO_MIB_VERSION_MAJOR}
        ${SSO_MIB_EXTERNAL_DIR}/install/lib/libsso-mib.so.${SSO_MIB_VERSION_MAJOR}.${SSO_MIB_VERSION_MINOR}.${SSO_MIB_VERSION_PATCH}
    )
  endif()
endif()

mark_as_advanced(SSO_MIB_INCLUDE_DIR SSO_MIB_LIBRARY)
