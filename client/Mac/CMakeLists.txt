
project(MacFreeRDP-library)

set(MODULE_NAME "MacFreeRDP-library")
set(MODULE_OUTPUT_NAME "MacFreeRDP")
set(MODULE_PREFIX "FREERDP_CLIENT_MAC_LIBRARY")

find_library(FOUNDATION_LIBRARY Foundation)
find_library(COCOA_LIBRARY Cocoa)
find_library(APPKIT_LIBRARY AppKit)

mark_as_advanced(COCOA_LIBRARY FOUNDATION_LIBRARY APPKIT_LIBRARY)
set(EXTRA_LIBS ${COCOA_LIBRARY} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY})

set(MACOSX_BUNDLE_INFO_STRING "${MODULE_OUTPUT_NAME}")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.freerdp.mac")
set(MACOSX_BUNDLE_BUNDLE_IDENTIFIER "FreeRDP-library.Mac")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "MacFreeRDP library Version 1.1")
set(MACOSX_BUNDLE_BUNDLE_NAME "${MODULE_OUTPUT_NAME}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING 1.1.0)
set(MACOSX_BUNDLE_BUNDLE_VERSION 1.1.0)
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2013. All Rights Reserved.")

set(${MODULE_PREFIX}_XIBS PasswordDialog.xib)

set(${MODULE_PREFIX}_SOURCES "")

set(${MODULE_PREFIX}_OBJECTIVE_SOURCES
	main.m
	mf_client.m
	MRDPCursor.m
	MRDPView.m
	PasswordDialog.m)

list(APPEND ${MODULE_PREFIX}_SOURCES ${${MODULE_PREFIX}_OBJECTIVE_SOURCES})

set(${MODULE_PREFIX}_HEADERS
	mfreerdp.h
	mf_client.h
	MRDPCursor.h
	MRDPView.h
	PasswordDialog.h)

set(${MODULE_PREFIX}_RESOURCES "en.lproj/InfoPlist.strings")

# Include XIB file in Xcode resources. 
if("${CMAKE_GENERATOR}" MATCHES "Xcode")
	message(STATUS "Adding Xcode XIB resources for ${MODULE_NAME}")
	set(${MODULE_PREFIX}_RESOURCES ${${MODULE_PREFIX}_RESOURCES} ${${MODULE_PREFIX}_XIBS})
endif()

add_library(${MODULE_NAME}
	SHARED
	../common/client.c
	${${MODULE_PREFIX}_SOURCES}
	${${MODULE_PREFIX}_HEADERS}
	${${MODULE_PREFIX}_RESOURCES})

set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "${MODULE_OUTPUT_NAME}")

# configures the framework to always be looked for in the application bundle in the Frameworks sub-folder.
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_INSTALL_PATH @executable_path/../Frameworks/)
          
set_target_properties(${MODULE_NAME} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${EXECUTABLE_OUTPUT_PATH} 
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${EXECUTABLE_OUTPUT_PATH})

set_target_properties(${MODULE_NAME} PROPERTIES 
	FRAMEWORK TRUE
	MACOSX_FRAMEWORK_IDENTIFIER com.awakecoding.${MODULE_NAME}
	FRAMEWORK_VERSION 1.1.0
	MACOSX_FRAMEWORK_SHORT_VERSION_STRING 1.1.0
	MACOSX_FRAMEWORK_BUNDLE_BUNDLE_VERSION 1.1.0
	INSTALL_NAME_DIR "@executable_path/../Frameworks"
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
	BUILD_WITH_INSTALL_RPATH 1)  

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${EXTRA_LIBS} freerdp-client)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE freerdp
	MODULES freerdp-core freerdp-cache freerdp-gdi freerdp-codec freerdp-primitives freerdp-rail freerdp-utils)

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS MONOLITHIC ${MONOLITHIC_BUILD}
	MODULE winpr
	MODULES winpr-input winpr-crt winpr-utils)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

# Set a list of the dependent targets used by the application. There should be a way to get this list automatically
# from cmake, but for now I put it down manually. I got the references by calling "otool -L MacFreeRDP-client"

set(DEPENDENCIES "")

foreach(LIB ${${MODULE_PREFIX}_LIBS})
	get_target_property(LOCATION ${LIB} LOCATION)
	if(NOT ${LOCATION} MATCHES ".*-NOTFOUND")
		list(APPEND DEPENDENCIES ${LIB})
	endif()
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Mac")

if (${BUILD_SHARED_LIBS})
	# Add a post-build event to copy the dependent libraries in the framework bundle 
	# Call install_name_tool to reassign the library install name
	foreach(LIB ${DEPENDENCIES})
		# message("adding post-build dependency: ${LIB}")
		add_custom_command(TARGET ${MODULE_NAME} POST_BUILD 
			COMMAND "${CMAKE_COMMAND}" -E copy 
					"$<TARGET_FILE:${LIB}>"
					"${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Contents/$<TARGET_FILE_NAME:${LIB}>"
			COMMENT "Copying ${LIB} to output directory"
			COMMAND install_name_tool -change "$<TARGET_SONAME_FILE:${LIB}>"
				"@executable_path/../Frameworks/${MODULE_OUTPUT_NAME}.framework/Contents/$<TARGET_FILE_NAME:${LIB}>"
				"${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/${MODULE_NAME}"
			COMMENT Setting install name for ${LIB}
			COMMAND "${CMAKE_COMMAND}" -E echo install_name_tool -change "$<TARGET_SONAME_FILE:${LIB}>"
				"@executable_path/../Frameworks/${MODULE_OUTPUT_NAME}.framework/Contents/$<TARGET_FILE_NAME:${LIB}>"
				"${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/${MODULE_NAME}")
	endforeach()

	# Call install_name_tool to reassign the library install names in dependent libraries
	foreach(DEST ${DEPENDENCIES})
		foreach(LIB ${DEPENDENCIES})
			# message("adding post-build dependency: ${LIB}")
			add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
				COMMAND install_name_tool -change "$<TARGET_SONAME_FILE:${LIB}>"
					"@executable_path/../Frameworks/${MODULE_OUTPUT_NAME}.framework/Contents/$<TARGET_FILE_NAME:${LIB}>"
					"${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Contents/$<TARGET_FILE_NAME:${DEST}>"
				COMMENT Setting install name for ${LIB} in module ${DEST})
		endforeach()
	endforeach()

endif()

# Add post-build NIB file generation in unix makefiles. XCode handles this implicitly.
# if("${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
	message(STATUS "Adding post-build NIB file generation event for ${MODULE_NAME}")

	# Make sure we can find the 'ibtool' program. If we can NOT find it we
	# skip generation of this project
	find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
	if (${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
		message(SEND_ERROR "ibtool can not be found and is needed to compile the .xib files. It should have been installed with 
				the Apple developer tools. The default system paths were searched in addition to ${OSX_DEVELOPER_ROOT}/usr/bin")
	endif()

	# Make sure the 'Resources' Directory is correctly created before we build
	add_custom_command(TARGET ${MODULE_NAME} PRE_BUILD
						  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Versions/${MACOSX_BUNDLE_SHORT_VERSION_STRING}/Resources)

	# Compile the .xib files using the 'ibtool' program with the destination being the app package
	foreach(xib ${${MODULE_PREFIX}_XIBS})
		get_filename_component(XIB_WE ${xib} NAME_WE)
						 
		add_custom_command (TARGET ${MODULE_NAME} POST_BUILD 
			COMMAND ${IBTOOL} --errors --warnings --notices --output-format human-readable-text 
				--compile ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Versions/${MACOSX_BUNDLE_SHORT_VERSION_STRING}/Resources/${XIB_WE}.nib ${CMAKE_CURRENT_SOURCE_DIR}/${xib}
			COMMENT "Compiling ${xib}")
	endforeach()
# endif()

# Copy the public header files into the framework
foreach(HEADER ${${MODULE_PREFIX}_HEADERS})
	# message("adding post-build dependency: ${LIB}")
	add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
		COMMAND ditto ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER} ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Headers/
		COMMENT Copying public header files to ${MODULE_NAME})
endforeach()

# Copy the FreeRDP header files into the framework
add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
	COMMAND ditto ${CMAKE_SOURCE_DIR}/include/freerdp ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Headers/freerdp
	COMMAND ditto ${CMAKE_SOURCE_DIR}/winpr/include/winpr ${CMAKE_CURRENT_BINARY_DIR}/$(CONFIGURATION)/${MODULE_OUTPUT_NAME}.framework/Headers/winpr
	COMMENT Copying FreeRDP header files to ${MODULE_NAME})

add_subdirectory(cli)
