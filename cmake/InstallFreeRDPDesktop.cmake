include(GNUInstallDirs)

option(WITH_INSTALL_CLIENT_DESKTOP_FILES "Install .desktop files for clients" OFF)

set(DESKTOP_RESOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../resources" CACHE INTERNAL "")

function(install_freerdp_desktop name)
  if(WITH_INSTALL_CLIENT_DESKTOP_FILES)
    get_target_property(FREERDP_APP_NAME ${name} OUTPUT_NAME)
    set(FREERDP_BIN_NAME "${CMAKE_INSTALL_FULL_BINDIR}/${FREERDP_APP_NAME}")
    set(FREERDP_DESKTOP_NAME "${CMAKE_CURRENT_BINARY_DIR}/${FREERDP_BIN_NAME}.desktop")
    set(FREERDP_DESKTOP_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${FREERDP_BIN_NAME}-file.desktop")
    configure_file(${DESKTOP_RESOURCE_DIR}/freerdp.desktop.template ${FREERDP_DESKTOP_NAME} @ONLY)
    configure_file(${DESKTOP_RESOURCE_DIR}/freerdp-file.desktop.template ${FREERDP_DESKTOP_FILE_NAME} @ONLY)
    install(FILES ${FREERDP_DESKTOP_NAME} ${FREERDP_DESKTOP_FILE_NAME}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES ${DESKTOP_RESOURCE_DIR}/FreeRDP_Icon.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps RENAME FreeRDP.svg
    )
  endif()
endfunction()
