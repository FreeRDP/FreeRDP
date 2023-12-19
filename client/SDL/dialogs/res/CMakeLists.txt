
add_executable(freerdp-res2bin
       convert_res_to_c.cpp
)

set(SRCS
    sdl_resource_manager.cpp
    sdl_resource_manager.hpp  
)

set(RES_SVG_FILES
    ${CMAKE_SOURCE_DIR}/resources/FreeRDP_Icon.svg
    ${CMAKE_SOURCE_DIR}/resources/error_FILL0_wght400_GRAD0_opsz24.svg
    ${CMAKE_SOURCE_DIR}/resources/feedback_FILL0_wght400_GRAD0_opsz24.svg
    ${CMAKE_SOURCE_DIR}/resources/warning_FILL0_wght400_GRAD0_opsz24.svg
)

set(RES_FONT_FILES
    ${CMAKE_SOURCE_DIR}/client/SDL/dialogs/font/OpenSans-VariableFont_wdth,wght.ttf
)

macro(convert_to_bin FILE FILE_TYPE)
    get_filename_component(FILE_NAME ${FILE} NAME)
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" TARGET_NAME ${FILE_NAME})

    set(FILE_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(FILE_BYPRODUCTS ${FILE_BIN_DIR}/${TARGET_NAME}.hpp ${FILE_BIN_DIR}/${TARGET_NAME}.cpp)

    list(APPEND SRCS
        ${FILE_BYPRODUCTS}
    )

    add_custom_command(
        OUTPUT ${FILE_BYPRODUCTS}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FILE_BIN_DIR}
        COMMAND $<TARGET_FILE:freerdp-res2bin> ${FILE} ${FILE_TYPE} ${TARGET_NAME} ${FILE_BIN_DIR}
        COMMENT "create image resources"
        DEPENDS freerdp-res2bin
        DEPENDS ${FILE}
    )
endmacro()

option(SDL_USE_COMPILED_RESOURCES "Compile in images/fonts" ON)

if (SDL_USE_COMPILED_RESOURCES)
    list(APPEND SRCS
        sdl_resource_file.cpp
        sdl_resource_file.hpp
    )

    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    foreach(FILE ${RES_SVG_FILES})
        convert_to_bin("${FILE}" "images")
    endforeach()

    foreach(FILE ${RES_FONT_FILES})
        convert_to_bin("${FILE}" "fonts")
    endforeach()
    add_definitions(-DSDL_USE_COMPILED_RESOURCES)
else()
    set(SDL_RESOURCE_ROOT ${CMAKE_INSTALL_FULL_DATAROOTDIR}/FreeRDP)
    add_definitions(-DSDL_RESOURCE_ROOT="${SDL_RESOURCE_ROOT}")

    install(
        FILES ${RES_SVG_FILES}
        DESTINATION ${SDL_RESOURCE_ROOT}/images
    )

    install(
        FILES ${RES_FONT_FILES}
        DESTINATION ${SDL_RESOURCE_ROOT}/fonts
    )
endif()

add_library(sdl_client_res OBJECT
    ${RES_FILES}
    ${SRCS}
)
set_property(TARGET sdl_client_res PROPERTY POSITION_INDEPENDENT_CODE ON)
