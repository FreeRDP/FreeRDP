find_package(PkgConfig QUIET)
pkg_check_modules(CAIRO QUIET libcairo)

find_path(CAIRO_INCLUDE_DIR
		  NAMES cairo.h
			PATHS ${CAIRO_INCLUDE_DIRS}
		  PATH_SUFFIXES cairo
		 )

# Finally the library itself
find_library(CAIRO_LIBRARY
			 NAMES cairo
			 PATHS ${CAIRO_LIBRARY_DIRS}
			)
mark_as_advanced(CAIRO_INCLUDE_DIR)
mark_as_advanced(CAIRO_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAIRO
	REQUIRED_VARS
		CAIRO_LIBRARY CAIRO_INCLUDE_DIR
	VERSION_VAR
		CAIRO_VERSION_STRING
	HANDLE_COMPONENTS)
