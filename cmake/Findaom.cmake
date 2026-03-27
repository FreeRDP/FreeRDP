# - Try to find the AOM library
# Once done this will define
#
#  AOM_ROOT - A list of search hints
#
#  AOM_FOUND - system has AOM
#  AOM_INCLUDE_DIRS - the AOM include directory
#  AOM_LIBRARIES - libaom library

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_AOM QUIET aom)
endif(UNIX AND NOT ANDROID)

if(AOM_INCLUDE_DIR AND AOM_LIBRARY)
  set(AOM_FIND_QUIETLY TRUE)
endif(AOM_INCLUDE_DIR AND AOM_LIBRARY)

find_path(AOM_INCLUDE_DIR NAMES aom/aom.h PATH_SUFFIXES include HINTS ${AOM_ROOT} ${PC_AOM_INCLUDE_DIRS})
find_library(AOM_LIBRARY NAMES aom PATH_SUFFIXES lib HINTS ${AOM_ROOT} ${PC_AOM_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AOM DEFAULT_MSG AOM_LIBRARY AOM_INCLUDE_DIR)

if(AOM_INCLUDE_DIR AND AOM_LIBRARY)
  set(AOM_FOUND ON CACHE BOOL "AOM")
  set(AOM_LIBRARIES ${AOM_LIBRARY} CACHE STRING "AOM")
  set(AOM_INCLUDE_DIRS ${AOM_INCLUDE_DIR} CACHE STRING "AOM")
endif(AOM_INCLUDE_DIR AND AOM_LIBRARY CACHE STRING "AOM")

if(AOM_FOUND)
  if(NOT AOM_FIND_QUIETLY)
    message(STATUS "Found AOM: ${AOM_LIBRARIES}")
  endif(NOT AOM_FIND_QUIETLY)
else(AOM_FOUND)
  if(AOM_FIND_REQUIRED)
    message(FATAL_ERROR "AOM was not found")
  endif(AOM_FIND_REQUIRED)
endif(AOM_FOUND)

mark_as_advanced(AOM_INCLUDE_DIRS AOM_LIBRARIES AOM_FOUND)
