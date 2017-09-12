
set(MODULE_NAME "DevolutionsRdp")
set(MODULE_PREFIX "DEVOLUTIONS_RDP")

set(OUTPUT_NAME "DevolutionsRdp")

set(FREERDP_NAME "FreeRDP")
set(FREERDP_VENDOR "Devolutions Inc.")
set(FREERDP_COPYRIGHT "Copyright 2017 ${FREERDP_VENDOR} All Rights Reserved.")

set(${MODULE_PREFIX}_HEADERS
	../../include/freerdp/api.h
	clipboard.h
	cursor.h
	DevolutionsRdp.h)

set(${MODULE_PREFIX}_SOURCES
	${${MODULE_PREFIX}_HDRS}
	clipboard.c
	headless.c
	cursor.c
	DevolutionsRdp.c)

set(${MODULE_PREFIX}_RESOURCES "")

if(WIN32)
	set(${MODULE_PREFIX}_RESOURCES
		DevolutionsRdp.rc)

	windows_rc_generate_version_info(
		NAME "${FREERDP_NAME}" TYPE "DLL"
		VERSION "${FREERDP_VERSION}"
		FILENAME "${OUTPUT_NAME}.dll"
		VENDOR "${FREERDP_VENDOR}"
		COPYRIGHT "${FREERDP_COPYRIGHT}"
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

	source_group("Resources" FILES ${${MODULE_PREFIX}_RESOURCES})
endif()

if(IOS)
	set(TARGET_TYPE "STATIC")
else()
	set(TARGET_TYPE "SHARED")
endif()

add_library(${MODULE_NAME} ${TARGET_TYPE} ${${MODULE_PREFIX}_HEADERS} ${${MODULE_PREFIX}_SOURCES} ${${MODULE_PREFIX}_RESOURCES})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "${OUTPUT_NAME}")

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP")
