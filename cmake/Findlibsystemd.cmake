# Module defines
#  LIBSYSTEMD_FOUND - libsystemd libraries and includes found
#  LIBSYSTEMD_INCLUDE_DIRS - the libsystemd include directories
#  LIBSYSTEMD_LIBRARIES - the libsystemd libraries
#
# Cache entries:
#   LIBSYSTEMD_LIBRARY      - detected libsystemd library
#   LIBSYSTEMD_INCLUDE_DIR   - detected libsystemd include dir(s)
#

if(LIBSYSTEMD_INCLUDE_DIR AND LIBSYSTEMD_LIBRARY)
    # in cache already
    set(LIBSYSTEMD_FOUND TRUE)
    set(LIBSYSTEMD_LIBRARIES ${LIBSYSTEMD_LIBRARY})
    set(LIBSYSTEMD_INCLUDE_DIRS ${LIBSYSTEMD_INCLUDE_DIR})
else()

    find_package(PkgConfig)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(_LIBSYSTEMD_PC QUIET "libsystemd")
	endif(PKG_CONFIG_FOUND)

	find_path(LIBSYSTEMD_INCLUDE_DIR systemd/sd-journal.h
			${_LIBSYSTEMD_PC_INCLUDE_DIRS}
			/usr/include
			/usr/local/include
	)
	mark_as_advanced(LIBSYSTEMD_INCLUDE_DIR)

	find_library (LIBSYSTEMD_LIBRARY NAMES systemd
			PATHS
			${_LIBSYSTEMD_PC_LIBDIR}
    )
    mark_as_advanced(LIBSYSTEMD_LIBRARY)

    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(libsystemd DEFAULT_MSG LIBSYSTEMD_LIBRARY LIBSYSTEMD_INCLUDE_DIR)

    if(libsystemd_FOUND)
        set(LIBSYSTEMD_LIBRARIES ${LIBSYSTEMD_LIBRARY})
        set(LIBSYSTEMD_INCLUDE_DIRS ${LIBSYSTEMD_INCLUDE_DIR})
    endif()

endif()
