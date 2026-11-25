# - Try to find the AVM library
# Once done this will define
#
#  AVM_ROOT - A list of search hints
#
#  AVM_FOUND - system has AVM
#  AVM_INCLUDE_DIRS - the AVM include directory
#  AVM_LIBRARIES - libavm library

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_AVM QUIET IMPORTED_TARGET GLOBAL avm)
endif(UNIX AND NOT ANDROID)

if(PC_AVM_FOUND)
  set(AVM_FOUND ${PC_AVM_FOUND})
  set(AVM_INCLUDE_DIRS ${PC_AVM_INCLUDE_DIRS})
  set(AVM_LIBRARIES ${PC_AVM_LIBRARIES})
  add_library(avm ALIAS PkgConfig::PC_AVM)
else()
  if(AVM_INCLUDE_DIR AND AVM_LIBRARY)
    set(AVM_FIND_QUIETLY TRUE)
  endif(AVM_INCLUDE_DIR AND AVM_LIBRARY)

  find_path(AVM_INCLUDE_DIR NAMES avm/avm.h PATH_SUFFIXES include HINTS ${AVM_ROOT} ${PC_AVM_INCLUDE_DIRS})
  find_library(AVM_LIBRARY NAMES avm PATH_SUFFIXES lib HINTS ${AVM_ROOT} ${PC_AVM_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(AVM DEFAULT_MSG AVM_LIBRARY AVM_INCLUDE_DIR)

  if(AVM_INCLUDE_DIR AND AVM_LIBRARY)
    set(AVM_FOUND ON CACHE BOOL "AVM")
    set(AVM_LIBRARIES ${AVM_LIBRARY} CACHE STRING "AVM")
    set(AVM_INCLUDE_DIRS ${AVM_INCLUDE_DIR} CACHE STRING "AVM")
  endif(AVM_INCLUDE_DIR AND AVM_LIBRARY CACHE STRING "AVM")

  if(AVM_FOUND)
    if(NOT AVM_FIND_QUIETLY)
      message(STATUS "Found AVM: ${AVM_LIBRARIES}")
    endif(NOT AVM_FIND_QUIETLY)
    add_library(avm INTERFACE IMPORTED GLOBAL)
    set_target_properties(
      avm PROPERTIES INTERFACE_LINK_LIBRARIES "${AVM_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${AVM_INCLUDE_DIR}"
    )
  else(AVM_FOUND)
    if(AVM_FIND_REQUIRED)
      message(FATAL_ERROR "AVM was not found")
    endif(AVM_FIND_REQUIRED)
  endif(AVM_FOUND)
endif()

mark_as_advanced(AVM_INCLUDE_DIRS AVM_LIBRARIES AVM_FOUND)
