# - Try to find libcbor
# Once done this will define
#  LIBCBOR_FOUND - libcbor was found
#  LIBCBOR_INCLUDE_DIRS - libcbor include directories
#  LIBCBOR_LIBRARIES - libraries needed for linking

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBCBOR QUIET libcbor)
endif()

find_path(LIBCBOR_INCLUDE_DIR cbor.h HINTS ${PC_LIBCBOR_INCLUDEDIR} ${PC_LIBCBOR_INCLUDE_DIRS})

find_library(LIBCBOR_LIBRARY NAMES cbor HINTS ${PC_LIBCBOR_LIBDIR} ${PC_LIBCBOR_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libcbor DEFAULT_MSG LIBCBOR_LIBRARY LIBCBOR_INCLUDE_DIR)

set(LIBCBOR_LIBRARIES ${LIBCBOR_LIBRARY})
set(LIBCBOR_INCLUDE_DIRS ${LIBCBOR_INCLUDE_DIR})

mark_as_advanced(LIBCBOR_INCLUDE_DIR LIBCBOR_LIBRARY)
