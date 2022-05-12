set(UWAC_INCLUDE_DIR "include/uwac${UWAC_VERSION_MAJOR}")
# cmake package
export(PACKAGE uwac)

SetFreeRDPCMakeInstallDir(UWAC_CMAKE_INSTALL_DIR "uwac${UWAC_VERSION_MAJOR}")

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/uwacConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/uwacConfig.cmake
	INSTALL_DESTINATION ${UWAC_CMAKE_INSTALL_DIR}
	PATH_VARS UWAC_INCLUDE_DIR)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/uwacConfigVersion.cmake
	VERSION ${UWAC_VERSION}
	COMPATIBILITY SameMajorVersion)

install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/uwacConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/uwacConfigVersion.cmake
	DESTINATION ${UWAC_CMAKE_INSTALL_DIR})

install(EXPORT uwac DESTINATION ${UWAC_CMAKE_INSTALL_DIR})

set(UWAC_BUILD_CONFIG_LIST "")
GET_CMAKE_PROPERTY(res VARIABLES)
FOREACH(var ${res})
	IF (var MATCHES "^WITH_*|^BUILD_TESTING|^HAVE_*")
		LIST(APPEND UWAC_BUILD_CONFIG_LIST "${var}=${${var}}")
	ENDIF()
ENDFOREACH()
string(REPLACE ";" " " UWAC_BUILD_CONFIG "${UWAC_BUILD_CONFIG_LIST}")
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/uwac/version.h)
configure_file(buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/uwac/buildflags.h)
configure_file(build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/uwac/build-config.h)
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/uwac/config.h)

configure_file(uwac.pc.in ${CMAKE_CURRENT_BINARY_DIR}/uwac${UWAC_VERSION_MAJOR}.pc @ONLY)

set(UWAC_INSTALL_INCLUDE_DIR ${UWAC_INCLUDE_DIR}/uwac)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uwac${UWAC_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
