
project(MacFreeRDP)

set(MODULE_NAME "MacFreeRDP")
set(MODULE_OUTPUT_NAME "MacFreeRDP")
set(MODULE_PREFIX "FREERDP_CLIENT_MAC_CLIENT")

# Import libraries
find_library(FOUNDATION_LIBRARY Foundation)
find_library(COCOA_LIBRARY Cocoa)
find_library(APPKIT_LIBRARY AppKit)

set(MACOSX_BUNDLE_INFO_STRING "MacFreeRDP")
set(MACOSX_BUNDLE_ICON_FILE "FreeRDP.icns")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.freerdp.mac")
set(MACOSX_BUNDLE_BUNDLE_IDENTIFIER "FreeRDP-client.Mac")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "MacFreeRDP Client Version 1.1.0")
set(MACOSX_BUNDLE_BUNDLE_NAME "MacFreeRDP")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING 1.1.0)
set(MACOSX_BUNDLE_BUNDLE_VERSION 1.1.0)
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2013. All Rights Reserved.")

set(MACOSX_BUNDLE_NSMAIN_NIB_FILE "MainMenu")
set(MACOSX_BUNDLE_NSPRINCIPAL_CLASS "NSApplication")

mark_as_advanced(COCOA_LIBRARY FOUNDATION_LIBRARY APPKIT_LIBRARY)
set(APP_TYPE MACOSX_BUNDLE)

set(${MODULE_PREFIX}_XIBS MainMenu.xib)

set(${MODULE_PREFIX}_SOURCES "")

set(${MODULE_PREFIX}_OBJECTIVE_SOURCES
	main.m
	AppDelegate.m)

list(APPEND ${MODULE_PREFIX}_SOURCES ${${MODULE_PREFIX}_OBJECTIVE_SOURCES})

set(${MODULE_PREFIX}_HEADERS
	AppDelegate.h)

set(${MODULE_PREFIX}_RESOURCES ${MACOSX_BUNDLE_ICON_FILE})

# Include XIB file in Xcode resources. 
if("${CMAKE_GENERATOR}" MATCHES "Xcode")
	message(STATUS "Adding Xcode XIB resources for ${MODULE_NAME}")
	set(${MODULE_PREFIX}_RESOURCES ${${MODULE_PREFIX}_RESOURCES} ${${MODULE_PREFIX}_XIBS})
endif()

add_executable(${MODULE_NAME}
	${APP_TYPE} 
	${${MODULE_PREFIX}_HEADERS}
	${${MODULE_PREFIX}_SOURCES}
	${${MODULE_PREFIX}_RESOURCES})

set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "${MODULE_OUTPUT_NAME}") 

# This is necessary for the xib file part below
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

# This allows for automatic xib to nib ibitool
set_target_properties(${MODULE_NAME} PROPERTIES RESOURCE "${${MODULE_PREFIX}_RESOURCES}")

# Tell the compiler where to look for the FreeRDP framework
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -F../")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -F../")

# Tell XCode where to look for the MacFreeRDP framework
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS
	"${XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS} ${CMAKE_CURRENT_BINARY_DIR}/../$(CONFIGURATION)")

# Set the info plist to the custom instance
set_target_properties(${MODULE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

# Disable transitive linking
target_link_libraries(${MODULE_NAME} ${COCOA_LIBRARY} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY} MacFreeRDP-library)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Mac")

# Embed the FreeRDP framework into the app bundle
add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
	COMMAND mkdir ARGS -p ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.app/Contents/Frameworks
	COMMAND ditto ${CMAKE_CURRENT_BINARY_DIR}/../$(CONFIGURATION)/MacFreeRDP.framework ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.app/Contents/Frameworks/MacFreeRDP.framework
	COMMAND install_name_tool -change "@executable_path/../Frameworks/MacFreeRDP.framework/Versions/${MAC_OS_X_BUNDLE_BUNDLE_VERSION}/MacFreeRDP"
		"@executable_path/../Frameworks/MacFreeRDP.framework/Versions/Current/MacFreeRDP"
		"${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.app/Contents/MacOS/${MODULE_NAME}"
	COMMENT Setting install name for MacFreeRDP)

# Add post-build NIB file generation in unix makefiles. XCode handles this implicitly.
if("${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
	message(STATUS "Adding post-build NIB file generation event for ${MODULE_NAME}")
	 
	# Make sure we can find the 'ibtool' program. If we can NOT find it we skip generation of this project
	find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
	if (${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
		message(SEND_ERROR "ibtool can not be found and is needed to compile the .xib files. It should have been installed with 
						the Apple developer tools. The default system paths were searched in addition to ${OSX_DEVELOPER_ROOT}/usr/bin")
	endif()

	# Make sure the 'Resources' Directory is correctly created before we build
	add_custom_command(TARGET ${MODULE_NAME} PRE_BUILD COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/\${CONFIGURATION}/${MODULE_OUTPUT_NAME}.app/Contents/Resources)

	# Compile the .xib files using the 'ibtool' program with the destination being the app package
	foreach(xib ${${MODULE_PREFIX}_XIBS})
		get_filename_component(XIB_WE ${xib} NAME_WE)
						 
		add_custom_command (TARGET ${MODULE_NAME} POST_BUILD 
			COMMAND ${IBTOOL} --errors --warnings --notices --output-format human-readable-text 
				--compile ${CMAKE_CURRENT_BINARY_DIR}/\${CONFIGURATION}/${MODULE_OUTPUT_NAME}.app/Contents/Resources/${XIB_WE}.nib ${CMAKE_CURRENT_SOURCE_DIR}/${xib}
			COMMENT "Compiling ${xib}")
	endforeach()

endif()
