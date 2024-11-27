# - Try to find JSON-C
# Once done this will define
#
#  JSONC_FOUND - JSON-C was found
#  JSONC_INCLUDE_DIRS - JSON-C include directories
#  JSONC_LIBRARIES - libraries needed for linking

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_JSONC json-c)
endif()

find_path(JSONC_INCLUDE_DIR NAMES json.h HINTS ${PC_JSONC_INCLUDE_DIRS} PATH_SUFFIXES json-c)
find_library(JSONC_LIBRARY NAMES json-c HINTS ${PC_JSONC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONC DEFAULT_MSG JSONC_LIBRARY JSONC_INCLUDE_DIR)

if(JSONC_FOUND)
  set(JSONC_LIBRARIES ${JSONC_LIBRARY})
  set(JSONC_INCLUDE_DIRS ${JSONC_INCLUDE_DIR})
endif()

mark_as_advanced(JSONC_LIBRARY JSONC_INCLUDE_DIR)
