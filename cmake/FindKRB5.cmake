# - Try to find krb5
# Once done this will define
#  KRB5_FOUND - pcsc was found
#  KRB5_INCLUDE_DIRS - pcsc include directories
#  KRB5_LIBRARIES - libraries needed for linking

include(FindPkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_KRB5 QUIET libkrb5)
endif()

find_path(KRB5_INCLUDE_DIR krb5.h
	HINTS ${PC_KRB5_INCLUDEDIR} ${PC_KRB5_INCLUDE_DIRS}
	PATH_SUFFIXES KRB5)

find_library(KRB5_LIBRARY NAMES krb5 
	HINTS ${PC_KRB5_LIBDIR} ${PC_KRB5_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KRB5 DEFAULT_MSG KRB5_LIBRARY KRB5_INCLUDE_DIR)

set(KRB5_LIBRARIES ${KRB5_LIBRARY})
set(KRB5_INCLUDE_DIRS ${KRB5_INCLUDE_DIR})

mark_as_advanced(KRB5_INCLUDE_DIR KRB5_LIBRARY)


