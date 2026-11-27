# - Try to find the libsmb2 library
# Once done this will define
#
#  LIBSMB2_ROOT - A list of search hints
#
#  libsmb2_FOUND - system has libsmb2
#  LIBSMB2_INCLUDE_DIRS - the libsmb2 include directory
#  LIBSMB2_LIBRARIES - libsmb2 library
#
# and the imported target libsmb2

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_LIBSMB2 QUIET libsmb2)
endif()

find_path(LIBSMB2_INCLUDE_DIR NAMES smb2/libsmb2.h PATH_SUFFIXES include HINTS ${LIBSMB2_ROOT}
                                                                               ${PC_LIBSMB2_INCLUDE_DIRS}
)
find_library(LIBSMB2_LIBRARY NAMES smb2 PATH_SUFFIXES lib HINTS ${LIBSMB2_ROOT} ${PC_LIBSMB2_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libsmb2 REQUIRED_VARS LIBSMB2_LIBRARY LIBSMB2_INCLUDE_DIR)

if(libsmb2_FOUND)
  set(LIBSMB2_LIBRARIES ${LIBSMB2_LIBRARY})
  set(LIBSMB2_INCLUDE_DIRS ${LIBSMB2_INCLUDE_DIR})
  if(NOT TARGET libsmb2)
    add_library(libsmb2 INTERFACE IMPORTED GLOBAL)
    set_target_properties(
      libsmb2 PROPERTIES INTERFACE_LINK_LIBRARIES "${LIBSMB2_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES
                                                                       "${LIBSMB2_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(LIBSMB2_INCLUDE_DIR LIBSMB2_LIBRARY)
