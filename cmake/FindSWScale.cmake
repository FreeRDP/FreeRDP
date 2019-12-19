
include(FindPkgConfig)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(SWScale libswscale)
endif()

find_path(SWScale_INCLUDE_DIR libswscale/swscale.h PATHS ${SWScale_INCLUDE_DIRS})
find_library(SWScale_LIBRARY swscale PATHS ${SWScale_LIBRARY_DIRS})

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SWScale DEFAULT_MSG SWScale_INCLUDE_DIR SWScale_LIBRARY)

mark_as_advanced(SWScale_INCLUDE_DIR SWScale_LIBRARY)

