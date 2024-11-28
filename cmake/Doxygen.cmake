option(WITH_DOCUMENTATION "Add target to build doxygen documentation" OFF)

if(WITH_DOCUMENTATION)
  if(CMAKE_VERSION VERSION_LESS "3.27")
    message(WARNING "Building with CMake ${CMAKE_VERSION} but >= 3.27 required for doxygen target")
  else()
    include(FindDoxygen)
    find_package(Doxygen REQUIRED dot OPTIONAL_COMPONENTS mscgen dia)

    set(DOXYGEN_PROJECT_NAME ${PROJECT_NAME})
    set(DOXYGEN_PROJECT_NUMBER ${PROJECT_VERSION})
    set(DOXYGEN_EXCLUDE_PATTERNS "*/uwac/protocols/*")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs")

    doxygen_add_docs(
      docs "${CMAKE_SOURCE_DIR}" ALL COMMENT "Generate doxygen docs"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/docs" CONFIG_FILE "${CMAKE_SOURCE_DIR}/docs/Doxyfile"
    )
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/docs/api" DESTINATION ${CMAKE_INSTALL_DOCDIR})
  endif()
endif()
