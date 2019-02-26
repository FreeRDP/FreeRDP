set(MODULE_NAME "pfreerdp")
set(MODULE_PREFIX "FREERDP_SERVER_PROXY")

set(${MODULE_PREFIX}_SRCS
	pfreerdp.c
	pfreerdp.h)

	# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-server)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
if (WITH_DEBUG_SYMBOLS AND MSVC)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/proxy")