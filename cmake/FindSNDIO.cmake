# - Try to find sndio
# Once done this will define
#  SNDIO_FOUND - sndio was found
#  SNDIO_INCLUDE_DIRS - sndio include directories
#  SNDIO_LIBRARIES - libraries needed for linking

find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SNDIO QUIET sndio)
endif()

find_path(SNDIO_INCLUDE_DIR sndio.h HINTS ${PC_SNDIO_INCLUDEDIR} ${PC_SNDIO_INCLUDE_DIRS})

find_library(SNDIO_LIBRARY NAMES sndio HINTS ${PC_SNDIO_LIBDIR} ${PC_SNDIO_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SNDIO DEFAULT_MSG SNDIO_LIBRARY SNDIO_INCLUDE_DIR)

set(SNDIO_LIBRARIES ${SNDIO_LIBRARY})
set(SNDIO_INCLUDE_DIRS ${SNDIO_INCLUDE_DIR})

mark_as_advanced(SNDIO_INCLUDE_DIR SNDIO_LIBRARY)
