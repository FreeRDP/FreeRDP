# - Find OpenSLES
# Find the OpenSLES includes and libraries
#
#  OPENSLES_INCLUDE_DIR - where to find dsound.h
#  OPENSLES_LIBRARIES   - List of libraries when using dsound.
#  OPENSLES_FOUND       - True if dsound found.

if(OPENSLES_INCLUDE_DIR)
	# Already in cache, be silent
	set(OPENSLES_FIND_QUIETLY TRUE)
else()
	find_package(PkgConfig)
	pkg_check_modules(PC_OPENSLES QUIET OpenSLES)
endif(OPENSLES_INCLUDE_DIR)
				
find_path(OPENSLES_INCLUDE_DIR SLES/OpenSLES.h
	HINTS ${PC_OPENSLES_INCLUDE_DIR})

find_library(OPENSLES_LIBRARY NAMES OpenSLES
	HINTS ${PC_OPENSLES_LIBDIR} ${PC_OPENSLES_LIBRARY_DIRS}
	PATH_SUFFIXES "lib64")

# Handle the QUIETLY and REQUIRED arguments and set OPENSL_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPENSLES DEFAULT_MSG
	OPENSLES_INCLUDE_DIR OPENSLES_LIBRARY)
						
if(OPENSLES_FOUND)
	set(OPENSLES_LIBRARIES ${OPENSLES_LIBRARY})
else(OPENSLES_FOUND)
	if (OpenSLES_FIND_REQUIRED)
		message(FATAL_ERROR "Could NOT find OPENSLES")
	endif()
endif(OPENSLES_FOUND)

mark_as_advanced(OPENSLES_INCLUDE_DIR OPENSLES_LIBRARY)
