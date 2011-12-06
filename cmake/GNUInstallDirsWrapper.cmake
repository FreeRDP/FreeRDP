# GNUInstallDirs is a relatively new cmake module, so wrap it to avoid errors
include(GNUInstallDirs OPTIONAL RESULT_VARIABLE GID_PATH)
if(GID_PATH STREQUAL "NOTFOUND")
	if(NOT DEFINED CMAKE_INSTALL_BINDIR)
		set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "user executables (bin)")
	endif()

	if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
		set(CMAKE_INSTALL_LIBDIR "lib${LIB_SUFFIX}" CACHE PATH "object code libraries (lib)")
	endif()

	mark_as_advanced(CMAKE_INSTALL_BINDIR CMAKE_INSTALL_LIBDIR)
endif()
