# - Try to find cJSON
# Once done this will define
#
#  cJSON_FOUND - cJSON was found
#  CJSON_INCLUDE_DIRS - cJSON include directories
#  CJSON_LIBRARIES - libraries needed for linking

find_package(PkgConfig REQUIRED)
pkg_check_modules(CJSON libcjson)

find_path(cJSON_INCLUDE_DIR NAMES cjson/cJSON.h)
find_library(cJSON_LIBRARY NAMES cjson)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cJSON DEFAULT_MSG cJSON_LIBRARY cJSON_INCLUDE_DIR)

if(cJSON_FOUND)
	set(cJSON_LIBRARIES ${cJSON_LIBRARY})
	set(cJSON_INCLUDE_DIRS ${cJSON_INCLUDE_DIR}/cJSON)
endif()

mark_as_advanced(cJSON_LIBRARY cJSON_INCLUDE_DIR)