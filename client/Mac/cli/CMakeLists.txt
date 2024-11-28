cmake_minimum_required(VERSION 3.13)

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(MacFreeRDP VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/)
include(CommonConfigOptions)

# Import libraries
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
find_library(COCOA_LIBRARY Cocoa REQUIRED)
find_library(APPKIT_LIBRARY AppKit REQUIRED)

string(TIMESTAMP VERSION_YEAR "%Y")
set(MACOSX_BUNDLE_INFO_STRING "MacFreeRDP")
set(MACOSX_BUNDLE_ICON_FILE "FreeRDP.icns")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.freerdp.mac")
set(MACOSX_BUNDLE_BUNDLE_IDENTIFIER "FreeRDP-client.Mac")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "MacFreeRDP Client Version ${FREERDP_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_NAME "MacFreeRDP")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${FREERDP_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${FREERDP_VERSION})
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2013-${VERSION_YEAR}. All Rights Reserved.")

set(MACOSX_BUNDLE_NSMAIN_NIB_FILE "MainMenu")
set(MACOSX_BUNDLE_NSPRINCIPAL_CLASS "NSApplication")

set(XIBS MainMenu.xib)

set(SOURCES "")

set(OBJECTIVE_SOURCES main.m AppDelegate.m)

list(APPEND SOURCES ${OBJECTIVE_SOURCES})

set(HEADERS AppDelegate.h)

set(RESOURCES "en.lproj/InfoPlist.strings" ${MACOSX_BUNDLE_ICON_FILE})
# Include XIB file in Xcode resources.
if("${CMAKE_GENERATOR}" MATCHES "Xcode")
  message(STATUS "Adding Xcode XIB resources for ${MODULE_NAME}")
  list(APPEND RESOURCES ${XIBS})
  set(IS_XCODE ON)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/PkgInfo.in ${CMAKE_CURRENT_BINARY_DIR}/PkgInfo @ONLY)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES} ${RESOURCES})

if(WITH_BINARY_VERSIONING)
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}${PROJECT_VERSION_MAJOR}")
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE "${RESOURCES}")
set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

target_link_libraries(
  ${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY} MacFreeRDP-library
)

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
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Info.plist DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PkgInfo DESTINATION ${CMAKE_INSTALL_PREFIX})
