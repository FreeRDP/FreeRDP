
include (SetFreeRDPCMakeInstallDir)

# cmake package
export(PACKAGE rdtk)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/rdtkConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/rdtkConfig.cmake
	INSTALL_DESTINATION ${RDTK_CMAKE_INSTALL_DIR}
	PATH_VARS RDTK_INCLUDE_DIR)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/rdtkConfigVersion.cmake
	VERSION ${RDTK_VERSION}
	COMPATIBILITY SameMajorVersion)

set(RDTK_BUILD_CONFIG_LIST "")
GET_CMAKE_PROPERTY(res VARIABLES)
FOREACH(var ${res})
	IF (var MATCHES "^WITH_*|^BUILD_TESTING|^HAVE_*")
		LIST(APPEND RDTK_BUILD_CONFIG_LIST "${var}=${${var}}")
	ENDIF()
ENDFOREACH()

string(REPLACE ";" " " RDTK_BUILD_CONFIG "${RDTK_BUILD_CONFIG_LIST}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/buildflags.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/build-config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rdtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc @ONLY)

set(RDTK_INSTALL_INCLUDE_DIR ${RDTK_INCLUDE_DIR}/rdtk)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/rdtkConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/rdtkConfigVersion.cmake
	DESTINATION ${RDTK_CMAKE_INSTALL_DIR})


