
set(MODULE_NAME "DevolutionsRdp")
set(MODULE_PREFIX "DEVOLUTIONS_RDP")

set(${MODULE_PREFIX}_HDRS
	../../include/freerdp/api.h
	clipboard.h
	cursor.h
	devolutionsrdp.h)

set(${MODULE_PREFIX}_SRCS
	${${MODULE_PREFIX}_HDRS}
	clipboard.c
	headless.c
	cursor.c
	devolutionsrdp.c)

if(APPLE)
	set(TARGET_TYPE "MODULE")
else()
	set(TARGET_TYPE "SHARED")
endif()

add_library(${MODULE_NAME} ${TARGET_TYPE} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP")
