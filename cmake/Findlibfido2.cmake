# - Try to find libfido2
# Once done this will define
#  LIBFIDO2_FOUND - libfido2 was found
#  LIBFIDO2_INCLUDE_DIRS - libfido2 include directories
#  LIBFIDO2_LIBRARIES - libraries needed for linking

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBFIDO2 QUIET libfido2)
endif()

find_path(LIBFIDO2_INCLUDE_DIR fido.h HINTS ${PC_LIBFIDO2_INCLUDEDIR} ${PC_LIBFIDO2_INCLUDE_DIRS})

find_library(LIBFIDO2_LIBRARY NAMES fido2 HINTS ${PC_LIBFIDO2_LIBDIR} ${PC_LIBFIDO2_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libfido2 DEFAULT_MSG LIBFIDO2_LIBRARY LIBFIDO2_INCLUDE_DIR)

set(LIBFIDO2_LIBRARIES ${LIBFIDO2_LIBRARY})
set(LIBFIDO2_INCLUDE_DIRS ${LIBFIDO2_INCLUDE_DIR})

mark_as_advanced(LIBFIDO2_INCLUDE_DIR LIBFIDO2_LIBRARY)
