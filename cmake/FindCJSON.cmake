# - Try to find CJSON
# Once done this will define
#  CJSON_FOUND - cJSON was found
#  CJSON_INCLUDE_DIRS - cJSON include directories
#  CJSON_LIBRARIES - cJSON libraries for linking

find_path(CJSON_INCLUDE_DIR
    NAMES cjson/cJSON.h)

find_library(CJSON_LIBRARY
    NAMES cjson)

if (CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
    set(CJSON_FOUND ON)
    set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
    set(CJSON_LIBRARIES ${CJSON_LIBRARY})
endif()

mark_as_advanced(CJSON_INCLUDE_DIRS CJSON_LIBRARIES)
