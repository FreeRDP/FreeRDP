if(NOT RDTK_FORCE_STATIC_BUILD)
  include(SetFreeRDPCMakeInstallDir)

  # cmake package
  export(PACKAGE rdtk)

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/rdtkConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/rdtkConfig.cmake
    INSTALL_DESTINATION ${RDTK_CMAKE_INSTALL_DIR} PATH_VARS RDTK_INCLUDE_DIR
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rdtkConfigVersion.cmake VERSION ${RDTK_VERSION} COMPATIBILITY SameMajorVersion
  )
endif()

set(RDTK_BUILD_CONFIG_LIST "")
get_cmake_property(res VARIABLES)
foreach(var ${res})
  if(var MATCHES "^WITH_*|^BUILD_TESTING*|^RDTK_HAVE_*")
    list(APPEND RDTK_BUILD_CONFIG_LIST "${var}=${${var}}")
  endif()
endforeach()

include(pkg-config-install-prefix)

string(REPLACE ";" " " RDTK_BUILD_CONFIG "${RDTK_BUILD_CONFIG_LIST}")
cleaning_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/version.h)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/buildflags.h
)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/build-config.h
)
cleaning_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/../include/rdtk/config.h)

if(NOT RDTK_FORCE_STATIC_BUILD)
  cleaning_configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/rdtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc @ONLY
  )

  set(RDTK_INSTALL_INCLUDE_DIR ${RDTK_INCLUDE_DIR}/rdtk)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rdtkConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/rdtkConfigVersion.cmake
          DESTINATION ${RDTK_CMAKE_INSTALL_DIR}
  )
endif()
