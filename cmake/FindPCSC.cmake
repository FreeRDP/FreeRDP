
include(FindPkgConfig)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(PCSC libpcsclite)
endif()

find_path(PCSC_INCLUDE_DIR pcsclite.h
	PATHS ${PCSC_INCLUDE_DIRS}
	PATH_SUFFIXES PCSC)

find_library(PCSC_LIBRARY pcsclite
	PATHS ${PCSC_LIBRARY_DIRS})

# Windows and Mac detection from http://www.cmake.org/Bug/print_bug_page.php?bug_id=11325
IF(NOT PCSC_FOUND)
	# Will find PC/SC headers both on Mac and Windows
	FIND_PATH(PCSC_INCLUDE_DIRS WinSCard.h)
	# PCSC library is for Mac, WinSCard library is for Windows
	FIND_LIBRARY(PCSC_LIBRARY NAMES PCSC WinSCard)
ENDIF(NOT PCSC_FOUND)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCSC DEFAULT_MSG PCSC_INCLUDE_DIR PCSC_LIBRARY)

mark_as_advanced(PCSC_INCLUDE_DIR PCSC_LIBRARY)

