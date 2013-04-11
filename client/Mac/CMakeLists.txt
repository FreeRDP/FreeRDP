
project(MacFreeRDP)

set(MODULE_NAME "MacFreeRDP")
set(MODULE_PREFIX "FREERDP_CLIENT_MAC")

set(FRAMEWORK_HEADERS_PATH /System/Library/Frameworks/Cocoa.framework/Versions/A/Headers/)
include_directories(${FRAMEWORK_HEADERS_PATH} /System/Library/Frameworks) 

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -mmacosx-version-min=10.4")
set(GUI_TYPE MACOSX_BUNDLE)

find_library(FOUNDATION_LIBRARY Foundation)
find_library(COCOA_LIBRARY Cocoa)
find_library(APPKIT_LIBRARY AppKit)

set(MACOSX_BUNDLE_INFO_STRING "MacFreeRDP")
set(MACOSX_BUNDLE_ICON_FILE "FreeRDP.icns")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.freerdp.mac")
set(MACOSX_BUNDLE_BUNDLE_IDENTIFIER "FreeRDP.Mac")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "MacFreeRDP Version 1.0.1")
set(MACOSX_BUNDLE_BUNDLE_NAME "MacFreeRDP")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0.1)
set(MACOSX_BUNDLE_BUNDLE_VERSION 1.0.1)
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2012. All Rights Reserved.")

set(MACOSX_BUNDLE_NSMAIN_NIB_FILE "MainMenu")
set(MACOSX_BUNDLE_NSPRINCIPAL_CLASS "NSApplication")

mark_as_advanced(COCOA_LIBRARY FOUNDATION_LIBRARY APPKIT_LIBRARY)
set(EXTRA_LIBS ${COCOA_LIBRARY} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY})
set(APP_TYPE MACOSX_BUNDLE)

set(${MODULE_NAME}_XIBS
	MainMenu.xib
	PasswordDialog.xib)

set(${MODULE_NAME}_RESOURCES
	${${MODULE_NAME}_XIBS}
	${MACOSX_BUNDLE_ICON_FILE})

set(${MODULE_NAME}_HEADERS
	AppDelegate.h
	MRDPCursor.h
	MRDPRailView.h
	MRDPRailWindow.h
	MRDPView.h
	MRDPWindow.h
	PasswordDialog.h)

set(${MODULE_NAME}_SOURCES
	main.m
	AppDelegate.m
	MRDPCursor.m
	MRDPRailView.m
	MRDPRailWindow.m
	MRDPView.m
	MRDPWindow.m
	PasswordDialog.m)

add_executable(${MODULE_NAME}
	${APP_TYPE} 
	${${MODULE_NAME}_HEADERS}
	${${MODULE_NAME}_SOURCES}
	${${MODULE_NAME}_RESOURCES})

# This is necessary for the xib file part below
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

# This allows for automatic xib to nib ibitool
set_target_properties(${MODULE_NAME} PROPERTIES RESOURCE "${${MODULE_NAME}_RESOURCES}")

# Automatic ref counting
# temporary turn off for x86_64 build issue
# set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)

# Support for automatic reference counting requires non-fragile abi.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-nonfragile-abi")

set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_ARCHS "$(NATIVE_ARCH_ACTUAL)")

# Set the info plist to the custom instance
set_target_properties(${MODULE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${EXTRA_LIBS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-cache freerdp-gdi freerdp-codec freerdp-primitives freerdp-rail freerdp-utils)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-input winpr-crt winpr-utils)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Mac")
