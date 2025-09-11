macro(generate_proxy_module_config)
  if(NOT BUILD_SHARED_LIBS)
    string(REPLACE "-" "_" PROJECT_SHORT_NAME_UNDERSCORE ${PROJECT_NAME})
    string(REPLACE "proxy_" "" PROJECT_SHORT_NAME_UNDERSCORE ${PROJECT_SHORT_NAME_UNDERSCORE})
    string(REPLACE "_plugin" "" PROJECT_SHORT_NAME_UNDERSCORE ${PROJECT_SHORT_NAME_UNDERSCORE})

    set(PROJECT_LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")

    include(pkg-config-install-prefix)
    cleaning_configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/../freerdp-proxy-module.pc.in
      ${CMAKE_CURRENT_BINARY_DIR}/freerdp-${PROJECT_NAME}${FREERDP_VERSION_MAJOR}.pc @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp-${PROJECT_NAME}${FREERDP_VERSION_MAJOR}.pc
            DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR}
    )
  endif()
endmacro()
