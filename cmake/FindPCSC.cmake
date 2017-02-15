# - Try to find PCSC
# Once done this will define
#  PCSC_FOUND - pcsc was found
#  PCSC_INCLUDE_DIRS - pcsc include directories
#  PCSC_LIBRARIES - libraries needed for linking

include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_PCSC QUIET libpcsclite)
endif()

find_path(PCSC_INCLUDE_DIR pcsclite.h WinSCard.h
	HINTS ${PC_PCSC_INCLUDEDIR} ${PC_PCSC_INCLUDE_DIRS}
	PATH_SUFFIXES PCSC)

find_library(PCSC_LIBRARY NAMES PCSC WinSCard pcsclite 
	HINTS ${PC_PCSC_LIBDIR} ${PC_PCSC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCSC DEFAULT_MSG PCSC_LIBRARY PCSC_INCLUDE_DIR)

set(PCSC_LIBRARIES ${PCSC_LIBRARY})
set(PCSC_INCLUDE_DIRS ${PCSC_INCLUDE_DIR})

mark_as_advanced(PCSC_INCLUDE_DIR PCSC_LIBRARY)


