cmake_minimum_required(VERSION 3.13)

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(MacFreeRDP-library VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

cmake_policy(SET CMP0026 OLD)
cmake_policy(SET CMP0045 OLD)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(CommonConfigOptions)

find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
find_library(COCOA_LIBRARY Cocoa REQUIRED)
find_library(APPKIT_LIBRARY AppKit REQUIRED)
find_library(IOKIT_LIBRARY IOKit REQUIRED)
find_library(COREGRAPHICS_LIBRARY CoreGraphics REQUIRED)

set(EXTRA_LIBS ${COCOA_LIBRARY} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY} ${IOKIT_LIBRARY})

set(XIBS CertificateDialog.xib PasswordDialog.xib)

set(SOURCES "")

set(OBJECTIVE_SOURCES
    main.m
    mf_client.m
    MRDPCursor.m
    MRDPView.m
    Keyboard.m
    Clipboard.m
    CertificateDialog.m
    PasswordDialog.m
)

list(APPEND SOURCES ${OBJECTIVE_SOURCES})

set(HEADERS
    mfreerdp.h
    mf_client.h
    MRDPCursor.h
    MRDPView.h
    Keyboard.h
    Clipboard.h
    CertificateDialog.h
    PasswordDialog.h
)

set(RESOURCES "en.lproj/InfoPlist.strings")

# Include XIB file in Xcode resources.
if("${CMAKE_GENERATOR}" MATCHES "Xcode")
  message(STATUS "Adding Xcode XIB resources for ${MODULE_NAME}")
  list(APPEND RESOURCES ${XIBS})
  set(IS_XCODE ON)
endif()

add_library(${PROJECT_NAME} ../common/client.c ${SOURCES} ${HEADERS} ${RESOURCES})

set(LIBS ${EXTRA_LIBS} freerdp-client)

target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBS})

set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE "${RESOURCES}")

if(NOT IS_XCODE)
  find_program(IBTOOL ibtool REQUIRED HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")

  # Compile the .xib files using the 'ibtool' program with the destination being the app package
  foreach(xib ${XIBS})
    get_filename_component(XIB_WE ${xib} NAME_WE)
    set(NIB ${CMAKE_CURRENT_BINARY_DIR}/${XIB_WE}.nib)
    list(APPEND NIBS ${NIB})

    add_custom_command(
      TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${IBTOOL} --errors --warnings --notices --output-format
                                                human-readable-text --compile ${NIB} ${CMAKE_CURRENT_SOURCE_DIR}/${xib}
      COMMENT "Compiling ${xib}"
    )
  endforeach()

  install(FILES ${NIBS} DESTINATION ${CMAKE_INSTALL_DATADIR})
endif()

install(TARGETS ${PROJECT_NAME} COMPONENT client RESOURCE DESTINATION ${CMAKE_INSTALL_DATADIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(cli)
