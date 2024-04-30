# - Try to find JSON-C
# Once done this will define
#
#  JSONC_FOUND - JSON-C was found
#  JSONC_INCLUDE_DIRS - JSON-C include directories
#  JSONC_LIBRARIES - libraries needed for linking

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_JSON json-c)
endif()

find_path(JSONC_INCLUDE_DIR NAMES json-c/json.h json/json.h)
find_library(JSONC_LIBRARY NAMES json-c)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONC DEFAULT_MSG JSONC_LIBRARY JSONC_INCLUDE_DIR)

if(JSONC_FOUND)
	set(JSONC_LIBRARIES ${JSONC_LIBRARY})
	set(JSONC_INCLUDE_DIRS ${JSONC_INCLUDE_DIR}/json-c)
endif()

mark_as_advanced(JSONC_LIBRARY JSONC_INCLUDE_DIR)
