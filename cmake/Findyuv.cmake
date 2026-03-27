# - Try to find the YUV library
# Once done this will define
#
#  YUV_ROOT - A list of search hints
#
#  YUV_FOUND - system has YUV
#  YUV_INCLUDE_DIRS - the YUV include directory
#  YUV_LIBRARIES - libyuv library

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_YUV QUIET yuv)
endif(UNIX AND NOT ANDROID)

if(YUV_INCLUDE_DIR AND YUV_LIBRARY)
  set(YUV_FIND_QUIETLY TRUE)
endif(YUV_INCLUDE_DIR AND YUV_LIBRARY)

find_path(YUV_INCLUDE_DIR NAMES libyuv.h PATH_SUFFIXES include HINTS ${YUV_ROOT} ${PC_YUV_INCLUDE_DIRS})
find_library(YUV_LIBRARY NAMES yuv PATH_SUFFIXES lib HINTS ${YUV_ROOT} ${PC_YUV_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YUV DEFAULT_MSG YUV_LIBRARY YUV_INCLUDE_DIR)

if(YUV_INCLUDE_DIR AND YUV_LIBRARY)
  set(YUV_FOUND ON CACHE BOOL "YUV")
  set(YUV_LIBRARIES ${YUV_LIBRARY} CACHE STRING "YUV")
  set(YUV_INCLUDE_DIRS ${YUV_INCLUDE_DIR} CACHE STRING "YUV")
endif(YUV_INCLUDE_DIR AND YUV_LIBRARY)

if(YUV_FOUND)
  if(NOT YUV_FIND_QUIETLY)
    message(STATUS "Found YUV: ${YUV_LIBRARIES}")
  endif(NOT YUV_FIND_QUIETLY)
else(YUV_FOUND)
  if(YUV_FIND_REQUIRED)
    message(FATAL_ERROR "YUV was not found")
  endif(YUV_FIND_REQUIRED)
endif(YUV_FOUND)

mark_as_advanced(YUV_INCLUDE_DIRS YUV_LIBRARIES YUV_FOUND)
